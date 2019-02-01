/*
Copyright (C) 2017- The University of Notre Dame
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// this struct has the same fields as golang struct  
// Task defined in workqueue-boss/pkg/task/task.go
typedef struct wqboss_task {
	int ID;
	struct hash_table *inputs;
	struct hash_table *outputs;
	char *exec_cmd;
	char *task_state;
} wqboss_task; 

struct wqboss_task *new_wqboss_task(int ID, struct hash_table *inputs, 
		struct hash_table *output, char *exec_cmd, char *task_state) {
	struct wqboss_task *wt = malloc(sizeof(wqboss_task));
	wt->ID = ID;
	wt->inputs = inputs;
	wt->outputs = outputs;
	wt->exec_cmd = exec_cmd;
	wt->task_state = task_state;
	return wt;
}

struct wqboss_task *new_wqboss_task_from_json(char *json_str) {
	// TODO
	struct wqboss_task *wt;
	return wt
}

static int is_boss_running = 0;
static int is_connected = 0;
static int port_num = 0;
static int is_connected = 0;
static int sockfd = 0;

void start_boss() {
}

int setup_connection() {
}

static batch_job_id_t batch_job_wqboss_submit (struct batch_queue * q, 
		const char *cmd, const char *extra_input_files, 
		const char *extra_output_files, struct jx *envlist, 
		const struct rmsummary *resources)
{
	// 1. start Boss if it is not running
	if (!is_boss_running) {
		// TODO
	}
	// 2. build socket connection if it has not been setup yet
	if (!is_connected) {
		// TODO
	}
	// 3. inform wqboss to execute a new task 
	
	return 0;
}

static batch_job_id_t batch_job_wqboss_wait (struct batch_queue * q, 
		struct batch_job_info * info, time_t stoptime)
{
	// wait timeout for receiving a complete task
	// TODO
	return 0;	
}

static int batch_queue_wqboss_remove (struct batch_queue *q, 
		batch_job_id_t id) {
	// inform wqboss to stop running task
	// TODO
	return 0
}

static int batch_queue_wqboss_create (struct batch_queue *q)
{
	strncpy(q->logfile, "wqboss.log", sizeof(q->logfile));
	batch_queue_set_feature(q, "batch_log_name", "%s.wqbosslog");
	return 0;
}

static int batch_queue_wqboss_free (struct batch_queue *q)
{
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
