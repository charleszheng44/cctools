/*
Copyright (C) 2019- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#include "batch_job.h"
#include "batch_job_internal.h"
#include "work_queue.h"
#include "work_queue_internal.h" /* EVIL */
#include "debug.h"
#include "path.h"
#include "stringtools.h"
#include "macros.h"
#include "rmsummary.h"
#include "jx.h"
#include "jx_parse.h"
#include "jx_print.h"
#include "xxmalloc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *task_state_ready = "READY";
static const char *task_state_done = "DONE";
static const char *task_state_fail = "FAIL";

// wqboss_task has the same fields as golang struct  
// Task defined in workqueue-boss/pkg/task/task.go
// Makeflow, Boss and Master transfer task infos to
// others by this struct
typedef struct wqboss_task {
	int ID;
	struct hash_table *inputs;
	struct hash_table *outputs;
	char *exec_cmd;
	char *task_state;
} wqboss_task; 

// new_wqboss_task generates a new wqboss_task 
// object
struct wqboss_task *new_wqboss_task(int ID, 
		struct hash_table *inputs, 
		struct hash_table *outputs, 
		char *exec_cmd, char *task_state) {
	struct wqboss_task *wt = malloc(sizeof(wqboss_task));
	wt->ID = ID;
	wt->inputs = inputs;
	wt->outputs = outputs;
	wt->exec_cmd = exec_cmd;
	wt->task_state = task_state;
	return wt;
}

void delete_wqboss_task(struct wqboss_task *wt) {
	// TODO
}

// jx_to_strval_hash_table generates a hash_table <char*, char*> from
// given jx object. 
// struct jx *->struct hash_table *<char*, char*>
static struct hash_table *jx_to_strval_hash_table(struct jx *jx_obj) {
    struct hash_table *ht = hash_table_create(0, 0);
    const char *key; 
    struct jx *value;
    void *i = NULL;

    while((key = jx_iterate_keys(jx_obj, &i))) {
        value = jx_get_value(&i);
        hash_table_insert(ht, key, (value->u).string_value);
    }  
    return ht;
}

// new_wqboss_task_from_json deserializes a json string to
// wqboss_task object
// const char *-> struct jx *-> struct task*
struct wqboss_task *new_wqboss_task_from_json(char *json_string) {
    struct jx *j1 = jx_parse_string((const char*)json_string);
    struct jx *input_jx = jx_lookup(j1, "inputs");
    struct jx *output_jx = jx_lookup(j1, "outputs");
    struct hash_table *input_ht = jx_to_strval_hash_table(input_jx);
    struct hash_table *output_ht = jx_to_strval_hash_table(output_jx);

    struct wqboss_task *t = new_wqboss_task((int)jx_lookup_integer(j1, "id"),
            input_ht, output_ht, (char *)jx_lookup_string(j1, "exec_cmd"),
            (char *)jx_lookup_string(j1, "task_state"));
	
    return t;
}

// jx_insert_strval_hash_table convert a hash_table <char*, char*> to
// a jx object and insert it to given jx 
static void jx_insert_strval_hash_table(struct jx *j, 
		char *key, struct hash_table *str_ht) {
    struct jx *new_j = xxcalloc(1, sizeof(*j));
    j->type = JX_OBJECT;
    
    char *new_key;
    void *value; 
    
    hash_table_firstkey(str_ht);
    while(hash_table_nextkey(str_ht, &new_key, &value) != 0) {
        jx_insert_string(new_j, new_key, (char *)value);
    }
    jx_insert(j, jx_string(key), new_j);
}

// wqboss_task_to_json_string serializes a wqboss_task object
// to json string
// struct task *-> struct jx *-> const char *
char *wqboss_task_to_json_string(struct wqboss_task *t) {
    struct jx *j = xxcalloc(1, sizeof(*j));
    j->type = JX_OBJECT;

    jx_insert_integer(j, "id", (jx_int_t)t->ID);
    jx_insert_strval_hash_table(j, "inputs", t->inputs);
    jx_insert_strval_hash_table(j, "outputs", t->outputs);
    jx_insert_string(j, "exec_cmd", t->exec_cmd);
    jx_insert_string(j, "state", t->task_state);

    char *ret =  jx_print_string(j);
    jx_delete(j);
    return ret;
}

static int is_boss_running = 0;
static int port_num = 0;
static int is_connected = 0;
static int sock_fd = 0;
static int id_counter = 0;

// BUF_SIZE is the size of the message transferred between 
// client and server
#define BUF_SIZE 512
#define BOSS_PORT 9876 
#define SOCK_READ_TIMEOUT 3

// TODO not implement yet
static void start_boss() {
}

// setup_connection builds connection between makeflow and
// Work Queue Boss
static int setup_connection() {
	struct sockaddr_in srv_addr;
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd <= 0) {
		debug(D_INFO, "fail to assign socket for makeflow\n");
		return -1;
	}
	// set read time out
	struct timeval tv;	
	tv.tv_sec = SOCK_READ_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

	bzero((char *) &srv_addr, sizeof(srv_addr));	
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(BOSS_PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr)<=0){
		debug(D_ERROR, "fail to convert 127.0.0.1 to binary form\n");
		return -1;
	}
	// setup connection
	// makeflow will be the client, heartbeat will be generated on
	// boss side
	if (connect(sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) { 
		debug(D_ERROR, "fail to connect to Boss"); 
		return -1;
	}

	return 0;
}

static struct hash_table *str_to_hash_table(const char *inp_str) {
	struct hash_table *ht = hash_table_create(0, 0);
	const char *delim = ",";
	char *ptr = strtok((char *)inp_str, delim);
	while (ptr) {
		ptr = strtok(NULL, delim);
		char *key = malloc(sizeof(ptr));
		strcpy(key, (const char *)ptr);
		hash_table_insert(ht, (const char *)key, "");
	}
	return ht;
}

static char *batch_job_to_json_string(int id, const char *inputs, 
		const char *outputs, const char *cmd) {
	// batch_job -> wqboss_task
	struct hash_table *inps = str_to_hash_table(inputs);
	struct hash_table *oups = str_to_hash_table(outputs);
	struct wqboss_task *wt = 
		new_wqboss_task(id, inps, oups, (char *)cmd, (char *)task_state_ready);
	// wqboss_task -> json_string 
	char *json_str = wqboss_task_to_json_string(wt);
	delete_wqboss_task(wt);
	return json_str;
}

static batch_job_id_t batch_job_wqboss_submit (struct batch_queue * q, 
		const char *cmd, const char *extra_input_files, 
		const char *extra_output_files, struct jx *envlist, 
		const struct rmsummary *resources) {
	// 1. start Boss if it is not running
	if (!is_boss_running) {
		start_boss();
		is_boss_running = 1;	
	}
	// 2. build socket connection if it has not been setup yet
	if (!is_connected) {
		if (setup_connection() == -1){
			return -1;			
		};
		is_connected = 1;
	}
	// 3. inform wqboss to execute a new task 
	char *json_str = batch_job_to_json_string(id_counter++,
			extra_input_files, extra_output_files, cmd);
	if (send(sock_fd, json_str, strlen(json_str), 0) == -1) {
		debug(D_ERROR, "fail to send json string through socket: %s", 
				strerror(errno));
		return -1;
	}
	return id_counter;
}

static batch_job_id_t batch_job_wqboss_wait (struct batch_queue * q, 
		struct batch_job_info * info, time_t stoptime) {
	// wait timeout for receiving a complete task
	char *buf = malloc(BUF_SIZE);
	if (read(sock_fd, buf, BUF_SIZE) < 0) {
		debug(D_ERROR, "fail to read from socket: %s", 
			 strerror(errno));
	}
	struct wqboss_task *wt = new_wqboss_task_from_json(buf);
	free(buf);
	if (strcmp(wt->task_state, task_state_done) != 0) {
		debug(D_INFO, "task %d done", wt->ID);
		return (batch_job_id_t)wt->ID;
	} 
	debug(D_ERROR, "task %d fail", wt->ID);
	return 0;	
}

static int batch_job_wqboss_remove (struct batch_queue *q, 
		batch_job_id_t id) {
	// inform wqboss to stop running task
	// TODO
	return 0;
}

static int batch_queue_wqboss_create (struct batch_queue *q) {
	strncpy(q->logfile, "wqboss.log", sizeof(q->logfile));
	batch_queue_set_feature(q, "batch_log_name", "%s.wqbosslog");
	return 0;
}

static int batch_queue_wqboss_free (struct batch_queue *q) {
	// close socket
	// stop Work Queue Boss
	return 0;
}

batch_queue_stub_port(wqboss);
batch_queue_stub_option_update(wqboss);

batch_fs_stub_chdir(wqboss);
batch_fs_stub_getcwd(wqboss);
batch_fs_stub_mkdir(wqboss);
batch_fs_stub_putfile(wqboss);
batch_fs_stub_rename(wqboss);
batch_fs_stub_stat(wqboss);
batch_fs_stub_unlink(wqboss);

const struct batch_queue_module batch_queue_wqboss = {
	BATCH_QUEUE_TYPE_WORK_QUEUE_BOSS,
	"wqboss",

	batch_queue_wqboss_create,
	batch_queue_wqboss_free,
	batch_queue_wqboss_port,
	batch_queue_wqboss_option_update,

	{
		batch_job_wqboss_submit,
		batch_job_wqboss_wait,
		batch_job_wqboss_remove,
	},

	{
		batch_fs_wqboss_chdir,
		batch_fs_wqboss_getcwd,
		batch_fs_wqboss_mkdir,
		batch_fs_wqboss_putfile,
		batch_fs_wqboss_rename,
		batch_fs_wqboss_stat,
		batch_fs_wqboss_unlink,
	},
};

/* vim: set noexpandtab tabstop=4: */
