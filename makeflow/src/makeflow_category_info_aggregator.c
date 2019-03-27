/*
Copyright (C) 2019- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#include "xxmalloc.h"
#include "dag.h"
#include "itable.h"
#include "hash_table.h"
#include "jx.h"
#include "jx_print.h"

// category_info holds the category information required by Kubernetes Operator
struct category_info  {
	char *name;
	struct list *nodes_id;
};

struct hash_table *generate_categories_info_from_dag(struct dag *d) {
    struct hash_table *categories_info = hash_table_create(0, 0);
    itable_firstkey(d->node_table);
    uint64_t key;
    void *val;
    while(itable_nextkey(d->node_table, &key, &val)) {
    	struct dag_node *cur_node = (struct dag_node *)val;	
    	void *tmp_val = hash_table_lookup(categories_info, 
    			cur_node->category->name);
    	int *tmp_key = malloc(sizeof(int));
    	*tmp_key = (int)key;
    	// task category already exist
    	if (tmp_val){
    		struct category_info *cat_info = (struct category_info*)tmp_val;
    		list_push_tail(cat_info->nodes_id, tmp_key);
    		continue;
    	}
    	// first task of this category
    	struct category_info *init_cat_info = malloc(sizeof(*init_cat_info));
    	init_cat_info->name = cur_node->category->name;
    	init_cat_info->nodes_id = list_create();
    	list_push_tail(init_cat_info->nodes_id, tmp_key);
    	hash_table_insert(categories_info, cur_node->category->name, 
    			init_cat_info);
    }
    return categories_info;
}

char *categories_info_to_json_string(struct hash_table *categories_info) {
    struct jx *j = xxcalloc(1, sizeof(*j));
    j->type = JX_OBJECT;
    hash_table_firstkey(categories_info);
    char *cat_info_key;
    void *cat_info;
    while(hash_table_nextkey(categories_info, &cat_info_key, &cat_info)) {
    	struct jx *jx_nodes_id = jx_array(0);
    	int num_tasks = list_size(((struct category_info*)cat_info)->nodes_id);
    	if (num_tasks != 0) {
    		list_first_item(((struct category_info*)cat_info)->nodes_id);
    		void *cur_id = NULL;
    		while ((cur_id = 
    				list_next_item(((struct category_info*)cat_info)->nodes_id))) {
    			jx_array_append(jx_nodes_id, jx_integer(*(int *)cur_id));
    		}	
    	} 
    	jx_insert(j, jx_string(cat_info_key), jx_nodes_id);
    }
    char *jx_cat_info_string = jx_print_string(j);
    jx_delete(j);
    return jx_cat_info_string;
}
	
void category_info_destroy(struct category_info *cat_info) {
    if (cat_info->nodes_id) {
        list_first_item(cat_info->nodes_id);
        void *cur_id = NULL;
        while((cur_id=list_next_item(cat_info->nodes_id))!= NULL) {
            free(cur_id);
        }
        list_destroy(cat_info->nodes_id);
    }
    free(cat_info);
}

void categories_info_delete(struct hash_table *cats_info) {
    if (cats_info == NULL) {
        return;
    }
    hash_table_firstkey(cats_info);
    char *key = NULL;
    void *val = NULL;
    while(hash_table_nextkey(cats_info, &key, &val)) {
        category_info_destroy((struct category_info *)val);
    }
    hash_table_delete(cats_info);
}
