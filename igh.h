#ifndef _IGH_H
#define _IGH_H

#include "ecrt.h"
#include "io.h"

typedef struct
{
	int position;

	ec_slave_info_t* info_p;
	ec_sync_info_t* input_sync_info_p;
	ec_sync_info_t* output_sync_info_p;
} igh_slave_t;

int igh_init(igh_slave_t** slave_list, int* slave_num);
int igh_mapping(io_mapping_info_t* mapping_list, int mapping_count);
int igh_activate(unsigned long long interval);
int igh_exchange(void);
int igh_cleanup(igh_slave_t** slave_list);

#endif
