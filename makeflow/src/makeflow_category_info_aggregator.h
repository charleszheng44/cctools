/*
Copyright (C) 2019- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#ifndef MAKEFLOW_CATEGORY_INFO_AGGREGATOR_H
#define MAKEFLOW_CATEGORY_INFO_AGGREGATOR_H

// generate_categories_info_from_dag aggregate nodes belong to same category
struct hash_table *generate_categories_info_from_dag(struct dag *);

// categories_info_to_json_string generate the json string of given hash_table
// of categories information
char *categories_info_to_json_string(struct hash_table *); 

// categories_info_delete delete the given hash_table of category_info
void categories_info_delete(struct hash_table *);

#endif	
