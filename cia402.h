#ifndef _CIA402_H
#define _CIA402_H

#include "igh.h"
#include "io.h"

typedef struct
{
	int position;

	int* power_control;
	int* power_feedback;
	int* scaled_target;
	double* scale_factor;

	int control_word;
	int status_word;
	int mode_of_operation;
	int target_position;

	char cw_address[15];
	char sw_address[15];
	char mo_address[15];
	char tp_address[15];
} cia402_node_t;

int cia402_get_node_list(igh_slave_t* slave_list, int slave_count,
	cia402_node_t** cia402_node_list, int* cia402_node_count);
int cia402_free_node_list(cia402_node_t** cia402_node_list);

int cia402_get_mapping_list(cia402_node_t* cia402_node_list, int cia402_node_count,
	io_mapping_info_t* mapping_list, int mapping_count,
	io_mapping_info_t** cia402_mapping_list, int* cia402_mapping_count);
int cia402_free_mapping_list(io_mapping_info_t** cia402_mapping_list);

int cia402_publish(cia402_node_t* cia402_node_list, int cia402_node_count);
int cia402_retrieve(cia402_node_t* cia402_node_list, int cia402_node_count);

#endif
