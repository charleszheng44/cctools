#ifndef _WORK_QUEUE_STRUCT_FOR_GO_H
#define _WORK_QUEUE_STRUCT_FOR_GO_H

#include <limits.h>

#define WORKER_ADDRPORT_MAX 32
#define WORKER_HASHKEY_MAX 32

struct work_queue {
	char *name;
	int port;
	int priority;
	int num_tasks_left;

	int next_taskid;

	char workingdir[PATH_MAX];

	struct link      *master_link;   // incoming tcp connection for workers.
	struct link_info *poll_table;
	int poll_table_size;

	struct itable *tasks;           // taskid -> task
	struct itable *task_state_map;  // taskid -> state
	struct list   *ready_list;      // ready to be sent to a worker

	struct hash_table *worker_table;
	struct hash_table *worker_blacklist;
	struct hash_table *worker_resizelist;
	struct itable  *worker_task_map;

	struct hash_table *categories;

	struct hash_table *workers_with_available_results;

	struct work_queue_stats *stats;
	struct work_queue_stats *stats_measure;
	struct work_queue_stats *stats_disconnected_workers;
	timestamp_t time_last_wait;

	int worker_selection_algorithm;
	int task_ordering;
	int process_pending_check;

	int short_timeout;		// timeout to send/recv a brief message from worker
	int long_timeout;		// timeout to send/recv a brief message from a foreman

	struct list *task_reports;	      /* list of last N work_queue_task_reports. */

	double asynchrony_multiplier;     /* Times the resource value, but disk */
	int    asynchrony_modifier;       /* Plus this many cores or unlabeled tasks */

	int minimum_transfer_timeout;
	int foreman_transfer_timeout;
	int transfer_outlier_factor;
	int default_transfer_rate;

	char *catalog_hosts;

	time_t catalog_last_update_time;
	time_t resources_last_update_time;
	int    busy_waiting_flag;

	category_mode_t allocation_default_mode;

	FILE *logfile;
	FILE *transactions_logfile;
	int keepalive_interval;
	int keepalive_timeout;
	timestamp_t link_poll_end;	//tracks when we poll link; used to timeout unacknowledged keepalive checks

    char *master_preferred_connection; 

	int monitor_mode;
	FILE *monitor_file;

	char *monitor_output_directory;
	char *monitor_summary_filename;

	char *monitor_exe;
	struct rmsummary *measured_local_resources;
	struct rmsummary *current_max_worker;

	char *password;
	double bandwidth;
};

struct work_queue_worker {
	char *hostname;
	char *os;
	char *arch;
	char *version;
	char addrport[WORKER_ADDRPORT_MAX];
	char hashkey[WORKER_HASHKEY_MAX];
	int  foreman;                             // 0 if regular worker, 1 if foreman
	struct work_queue_stats     *stats;
	struct work_queue_resources *resources;
	struct hash_table           *features;

	char *workerid;

	struct hash_table *current_files;
	struct link *link;
	struct itable *current_tasks;
	struct itable *current_tasks_boxes;
	int finished_tasks;
	int64_t total_tasks_complete;
	int64_t total_bytes_transferred;
	timestamp_t total_task_time;
	timestamp_t total_transfer_time;
	timestamp_t start_time;
	timestamp_t last_msg_recv_time;
	timestamp_t last_update_msg_time;
};

#endif