/* work_queue.i */
%module work_queue

%{
	#include "debug.h"
	#include "int_sizes.h"
	#include "timestamp.h"
	#include "work_queue.h"
	#include "rmsummary.h"
    #include "hash_table.h"
    #include "itable.h"
    #include "work_queue_struct_for_go.h"
%}

%typemap(in) off_t = int;

/*Function name to camelcase which follows Golang convention*/
%rename("%(camelcase)s") "";

/* vdebug() takes va_list as arg but SWIG can't wrap such functions. */
%ignore vdebug;
%ignore debug;

/* These return pointers to lists defined in list.h. We aren't
 * wrapping methods in list.h and so ignore these. */
%ignore work_queue_cancel_all_tasks;
%ignore input_files;
%ignore output_files;

%include "stdint.i"
%include "debug.h"
%include "int_sizes.h"
%include "timestamp.h"
%include "work_queue.h"
%include "rmsummary.h"
%include "category.h"
%include "hash_table.h"
%include "itable.h"
%include "work_queue_struct_for_go.h"