/*
Copyright (C) 2019- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#include "batch_job.h"
#include "batch_job_internal.h"
#include "debug.h"
#include "path.h"
#include "stringtools.h"
#include "rmsummary.h"
#include "jx.h"
#include "jx_parse.h"
#include "jx_print.h"
#include "xxmalloc.h"
#include "link.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// k8s_oper_task has the same fields as golang struct  
// Task defined in makeflow-k8s-operator/pkg/task/task.go
// Makeflow, Makeflow-k8s Operator and Master transfer 
// task infos to others by this struct
typedef struct k8s_oper_task {
	int ID;
	struct hash_table *inputs;
	struct hash_table *outputs;
	char *exec_cmd;
	char *category_name;	
	int64_t req_cores;
	int64_t req_CPU;
	int64_t req_mem;
	int64_t req_disk;
	int ret_code;
} k8s_oper_task; 

// safe_free checks if a pointer is empty, before
// free the memory it points to
static void safe_free(void *ptr) {
	if (ptr) {free(ptr);}
}

// new_k8s_oper_task generates a new k8s_oper_task object
struct k8s_oper_task *new_k8s_oper_task(int ID, 
		struct hash_table *inputs, 
		struct hash_table *outputs, 
		char *exec_cmd, const char *category_name,
		int64_t req_cores, int64_t req_CPU,
		int64_t req_mem, int64_t req_disk,
		int ret_code) {
	struct k8s_oper_task *wt = malloc(sizeof(k8s_oper_task));
	wt->ID = ID;
	wt->inputs = inputs;
	wt->outputs = outputs;
	wt->exec_cmd = exec_cmd;
	wt->category_name = category_name,
	wt->req_cores = req_cores;
	wt->req_CPU = req_CPU;
	wt->req_mem = req_mem;
	wt->req_disk = req_disk;
	wt->ret_code = ret_code;
	return wt;
}

void delete_k8s_oper_task(struct k8s_oper_task *wt) {
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

// new_k8s_oper_task_from_json deserializes a json string to
// k8s_oper_task object
// const char *-> struct jx *-> struct task*
struct k8s_oper_task *new_k8s_oper_task_from_json(char *json_string) {
    struct jx *j1 = jx_parse_string((const char*)json_string);
    struct jx *input_jx = jx_lookup(j1, "inputs");
    struct jx *output_jx = jx_lookup(j1, "outputs");
    struct hash_table *input_ht = jx_to_strval_hash_table(input_jx);
    struct hash_table *output_ht = jx_to_strval_hash_table(output_jx);
    struct k8s_oper_task *t = new_k8s_oper_task(
			(int)jx_lookup_integer(j1, "id"),
            input_ht, output_ht, 
			(char *)jx_lookup_string(j1, "execcmd"),
			(const char *)jx_lookup_string(j1, "categoryname"),
			jx_lookup_integer(j1, "reqcores"),
			jx_lookup_integer(j1, "reqCPU"),
			jx_lookup_integer(j1, "reqmem"),
			jx_lookup_integer(j1, "reqdisk"),
            (int)jx_lookup_integer(j1, "retcode"));
	
    return t;
}

// jx_insert_strval_hash_table convert a hash_table <char*, int*> to
// a jx object and insert it to given jx 
static void jx_insert_strval_hash_table(struct jx *j, 
		char *key, struct hash_table *str_ht) {
    struct jx *new_j = xxcalloc(1, sizeof(*new_j));
    new_j->type = JX_OBJECT;
    
    char *new_key;
    void *value; 
    
    hash_table_firstkey(str_ht);
    while(hash_table_nextkey(str_ht, &new_key, &value) != 0) {
		jx_insert(new_j, jx_string(new_key), (struct jx *)(value));
    }
    jx_insert(j, jx_string(key), new_j);
}

// k8s_oper_task_to_json_string serializes a k8s_oper_task object
// to json string
// struct task *-> struct jx *-> const char *
char *k8s_oper_task_to_json_string(struct k8s_oper_task *t) {
    struct jx *j = xxcalloc(1, sizeof(*j));
    j->type = JX_OBJECT;

    jx_insert_integer(j, "id", (jx_int_t)t->ID);
    jx_insert_strval_hash_table(j, "inputs", t->inputs);
    jx_insert_strval_hash_table(j, "outputs", t->outputs);
    jx_insert_string(j, "execcmd", t->exec_cmd);
	if (t->category_name) {
		jx_insert_string(j, "categoryname", t->category_name);
	} else {
		jx_insert_string(j, "categoryname", "");
	}
	jx_insert_integer(j, "reqcores", t->req_cores);
	jx_insert_integer(j, "reqCPU", t->req_CPU);
	jx_insert_integer(j, "reqmem", t->req_mem);
	jx_insert_integer(j, "reqdisk", t->req_disk);
	jx_insert_integer(j, "retcode", t->ret_code);

    char *ret =  jx_print_string(j);
    jx_delete(j);
    return ret;
}

static int is_k8s_oper_running = 0;
static int is_connected = 0;
static int id_counter = 0;
struct link *k8s_oper_link = NULL;

// BUF_SIZE is the size of the message transferred between 
// client and server
#define BUF_SIZE 2048
#define K8S_OPER_PORT 9876 
#define K8S_OPER_CONN_TIMEOUT 5
#define SOCK_READ_TIMEOUT 4

// TODO NOT IMPLEMENT
static void start_k8s_oper() {
}

// setup_k8s_oper_link builds connection between makeflow and
// makeflow-k8s operator.
// NOTE: makeflow act as a client, and keepalive will be set 
// on makeflow-k8s operator side
static int setup_k8s_oper_link() {
	time_t stop_time = time(0) + K8S_OPER_CONN_TIMEOUT;
	if((k8s_oper_link = link_connect("127.0.0.1", K8S_OPER_PORT, stop_time)) == 0) {
		debug(D_BATCH, "fail to connect to makeflow-k8s operator");
		return 0;
	}
	return 1;
}

static struct hash_table *str_to_hash_table(const char *inp_str) {
	struct hash_table *ht = hash_table_create(0, 0);
	const char *delim = ",";
	char *ptr = strtok((char *)inp_str, delim);
	while (ptr) {
		// char *key = malloc(sizeof(ptr));
		// strcpy(key, (const char *)ptr);
		struct jx *bool_false = jx_boolean(0);
		hash_table_insert(ht, (const char *)ptr, bool_false);
		ptr = strtok(NULL, delim);
	}
	return ht;
}

static char *batch_job_to_json_string(int id, const char *inputs, 
		const char *outputs, const char *cmd,
		const char *category_name, int64_t cores,
		int64_t CPU, int64_t memory, int64_t disk) {
	// batch_job -> k8s_oper_task
	struct hash_table *inps = str_to_hash_table(inputs);
	struct hash_table *oups = str_to_hash_table(outputs);
	struct k8s_oper_task *wt = 
		new_k8s_oper_task(id, inps, oups, (char *)cmd, 
				category_name, cores, CPU, memory, disk, 0);
	// k8s_oper_task -> json_string 
	char *json_str = k8s_oper_task_to_json_string(wt);
	delete_k8s_oper_task(wt);
	return json_str;
}

static batch_job_id_t batch_job_k8s_oper_submit (struct batch_queue *q, 
		const char *cmd, const char *extra_input_files, 
		const char *extra_output_files, struct jx *envlist, 
		const struct rmsummary *resources) {
	
	// 1. start makeflow-k8s operator if it is not running
	if (!is_k8s_oper_running) {
		start_k8s_oper();
		is_k8s_oper_running = 1;	
	}
	// 2. build socket connection if it has not been setup yet
	if (!is_connected) {
		if (!setup_k8s_oper_link()){
			return -1;			
		};
		is_connected = 1;
	}

	// get task category
	const char *task_cat = batch_queue_get_option(q, "task-cat");
	// 4. inform makeflow-k8s operator to execute a new task 
	char *json_str = batch_job_to_json_string(++id_counter,
			extra_input_files, extra_output_files, cmd,
			task_cat, resources->cores,
			resources->cpu_time, resources->memory,
			resources->disk);
	char *sock_msg = string_format("TASK_INFO %s\n", json_str);
	debug(D_BATCH, "the sock_msg is: %s", sock_msg);
	time_t stop_time = time(0) + K8S_OPER_CONN_TIMEOUT;
	if (link_write(k8s_oper_link, sock_msg, strlen(sock_msg), stop_time) < 0) {
		safe_free(sock_msg);
		debug(D_BATCH, "fail to send task information through socket: %s", 
				strerror(errno));
		return -1;
	}
	safe_free(sock_msg);
	return id_counter;
}

static batch_job_id_t batch_job_k8s_oper_wait (struct batch_queue * q, 
		struct batch_job_info * info, time_t stoptime) {
	// wait timeout for receiving a complete task
	char *buf = malloc(BUF_SIZE);
	time_t stop_time = time(0) + K8S_OPER_CONN_TIMEOUT;
	if (!link_readline(k8s_oper_link, buf, BUF_SIZE, stop_time)) {
		// debug(D_BATCH, "read nothing from socket: %s", strerror(errno));
		safe_free(buf);
		return 0;
	}
	
	debug(D_BATCH, "receive message from Boss: %s", buf);
	struct k8s_oper_task *wt = new_k8s_oper_task_from_json(buf);
	safe_free(buf);
	debug(D_BATCH, "task %d complete", wt->ID);
	info->exited_normally = 1;
	info->exit_code = wt->ret_code;
	return (batch_job_id_t)wt->ID;
}

static int batch_job_k8s_oper_remove (struct batch_queue *q, 
		batch_job_id_t id) {
	// TODO NOT IMPLEMENT
	// inform makeflow-k8s operator to stop running task
	return 0;
}

static int batch_queue_k8s_oper_create (struct batch_queue *q) {
	strncpy(q->logfile, "k8s_oper.log", sizeof(q->logfile));
	batch_queue_set_feature(q, "batch_log_name", "%s.k8soperlog");
	return 0;
}

static int batch_queue_k8s_oper_free (struct batch_queue *q) {
	// TODO NOT IMPLEMENT
	// close socket
	// stop makeflow-k8s operator 
	return 0;
}

batch_queue_stub_port(k8s_oper);
batch_queue_stub_option_update(k8s_oper);

batch_fs_stub_chdir(k8s_oper);
batch_fs_stub_getcwd(k8s_oper);
batch_fs_stub_mkdir(k8s_oper);
batch_fs_stub_putfile(k8s_oper);
batch_fs_stub_rename(k8s_oper);
batch_fs_stub_stat(k8s_oper);
batch_fs_stub_unlink(k8s_oper);

const struct batch_queue_module batch_queue_k8s_oper = {
	BATCH_QUEUE_TYPE_K8S_OPER,
	"k8s-oper",

	batch_queue_k8s_oper_create,
	batch_queue_k8s_oper_free,
	batch_queue_k8s_oper_port,
	batch_queue_k8s_oper_option_update,

	{
		batch_job_k8s_oper_submit,
		batch_job_k8s_oper_wait,
		batch_job_k8s_oper_remove,
	},

	{
		batch_fs_k8s_oper_chdir,
		batch_fs_k8s_oper_getcwd,
		batch_fs_k8s_oper_mkdir,
		batch_fs_k8s_oper_putfile,
		batch_fs_k8s_oper_rename,
		batch_fs_k8s_oper_stat,
		batch_fs_k8s_oper_unlink,
	},
};

/* vim: set noexpandtab tabstop=4: */
