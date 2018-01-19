#include "igh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecrt.h"
#include "io.h"

typedef struct
{
	void* variable;
	int size;

	unsigned int offset;
	unsigned int bit_pos;
	unsigned int bit_length;
} igh_value_t;

static ec_master_t* master = NULL;
static ec_domain_t* domain1 = NULL;
static ec_slave_info_t* slave_info_list = NULL;
static int slave_count = 0;

static uint8_t* domain1_pd = NULL;

static ec_pdo_entry_reg_t* pdo_entry_reg = NULL;
static ec_sync_info_t* input_sync_info_list = NULL;
static ec_sync_info_t* output_sync_info_list = NULL;

static int input_count = 0;
static int output_count = 0;
static igh_value_t* input_list = NULL;
static igh_value_t* output_list = NULL;

static unsigned int get_pdo_bit_length(uint16_t slave, uint16_t index, uint8_t subindex, int direction); 
static void free_sync_info_list(ec_sync_info_t* sync_info_list);
static void clear_inout_list();

int igh_init(igh_slave_t** slave_list, int* slave_num)
{
	int i, j, k, l;
	int ret = 0;

	ec_master_info_t master_info;
	ec_slave_config_t* slave = NULL;
	ec_sync_info_t temp_sync_info;
	ec_sync_info_t* target_sync_info = NULL;

	*slave_list = NULL;
	*slave_num = 0;

	if(master != NULL)
		return 1;

	/* configure master */
	master = ecrt_request_master(0);
	if(master == NULL)
	{
		printf("EtherCAT master request failed!\n");
		return 1;
	}

	domain1 = ecrt_master_create_domain(master);
	if(domain1 == NULL)
	{
		printf("EtherCAT domain creation failed!\n");
		igh_cleanup(slave_list);
		return 1;
	}

	ret = ecrt_master(master, &master_info);
	if(ret != 0)
	{
		printf("EtherCAT master information request failed!\n");
		igh_cleanup(slave_list);
		return -ret;
	}

	/* allocate momory for slave information and configuration */
	slave_count = master_info.slave_count;
	slave_info_list = (ec_slave_info_t*)malloc(sizeof(ec_slave_info_t) * slave_count);
	input_sync_info_list = (ec_sync_info_t*)malloc(sizeof(ec_sync_info_t) * slave_count);
	output_sync_info_list = (ec_sync_info_t*)malloc(sizeof(ec_sync_info_t) * slave_count);
	memset(input_sync_info_list, 0, sizeof(ec_sync_info_t) * slave_count);
	memset(output_sync_info_list, 0, sizeof(ec_sync_info_t) * slave_count);

	/* allocate momory for slave list */
	*slave_num = slave_count;
	*slave_list = (igh_slave_t*)malloc(sizeof(igh_slave_t) * slave_count);

	/* configure slaves */
	for(i = 0; i < slave_count; i++)
	{
		ret = ecrt_master_get_slave(master, i, &slave_info_list[i]);
		if(ret != 0)
		{
			printf("EtherCAT slave information request failed!\n");
			igh_cleanup(slave_list);
			return -ret;
		}

		slave = ecrt_master_slave_config(master, 0, i, slave_info_list[i].vendor_id, slave_info_list[i].product_code);
		if(slave == NULL)
		{
			printf("EtherCAT slave configuration failed!\n");
			igh_cleanup(slave_list);
			return 1;
		}

		/* get PDO structure */
		for(j = 0; j < slave_info_list[i].sync_count; j++)
		{
			ret = ecrt_master_get_sync_manager(master, i, j, &temp_sync_info);
			if(ret != 0)
			{
				printf("EtherCAT getting sync structure failed!\n");
				igh_cleanup(slave_list);
				return ret;
			}

			target_sync_info = NULL;
			if(temp_sync_info.dir == EC_DIR_INPUT && temp_sync_info.n_pdos != 0)
			{
				input_sync_info_list[i] = temp_sync_info;
				target_sync_info = &input_sync_info_list[i];
			}
			if(temp_sync_info.dir == EC_DIR_OUTPUT && temp_sync_info.n_pdos != 0)
			{
				output_sync_info_list[i] = temp_sync_info;
				target_sync_info = &output_sync_info_list[i];
			}

			if(target_sync_info != NULL)
			{
				target_sync_info -> pdos = (ec_pdo_info_t*)malloc(sizeof(ec_pdo_info_t) * target_sync_info -> n_pdos);
				for(k = 0; k < target_sync_info -> n_pdos; k++)
				{
					ret = ecrt_master_get_pdo(master, i, j, k, &(target_sync_info -> pdos[k]));
					if(ret != 0)
					{
						printf("EtherCAT getting PDO structure failed!\n");
						igh_cleanup(slave_list);
						return ret;
					}

					target_sync_info -> pdos[k].entries =
						(ec_pdo_entry_info_t*)malloc(sizeof(ec_pdo_entry_info_t) * target_sync_info -> pdos[k].n_entries);

					for(l = 0; l < target_sync_info -> pdos[k].n_entries; l++)
					{
						ret = ecrt_master_get_pdo_entry(master, i, j, k, l, &(target_sync_info -> pdos[k].entries[l]));
						if(ret != 0)
						{
							printf("EtherCAT getting PDO entry structure failed!\n");
							igh_cleanup(slave_list);
							return ret;
						}
					}
				}
			}

			if(input_sync_info_list[i].pdos != NULL && output_sync_info_list[i].pdos != NULL)
				break;
		}

		(*slave_list)[i].position = i;
		(*slave_list)[i].info_p = &slave_info_list[i];
		(*slave_list)[i].input_sync_info_p = &input_sync_info_list[i];
		(*slave_list)[i].output_sync_info_p = &output_sync_info_list[i];
	}

	return 0;
}

int igh_mapping(io_mapping_info_t* mapping_list, int mapping_count)
{
	int i, ret;
	int ic = 0;
	int oc = 0;
	igh_value_t* temp_target = NULL;

	int slave, index, subindex;

	if(input_count != 0 || output_count != 0)
		clear_inout_list();

	for(i = 0; i < mapping_count; i++)
	{
		if(mapping_list[i].direction == 1)
			input_count++;
		else if(mapping_list[i].direction == 0)
			output_count++;
	}

	pdo_entry_reg = (ec_pdo_entry_reg_t*)malloc(sizeof(ec_pdo_entry_reg_t) * (mapping_count + 1));
	input_list = (igh_value_t*)malloc(sizeof(igh_value_t) * input_count);
	output_list = (igh_value_t*)malloc(sizeof(igh_value_t) * output_count);

	for(i = 0; i < mapping_count; i++)
	{
		if(mapping_list[i].direction == 1)
			temp_target = &input_list[ic++];
		else if(mapping_list[i].direction == 0)
			temp_target = &output_list[oc++];

		temp_target -> variable = mapping_list[i].model_addr;
		temp_target -> size = mapping_list[i].size;

		sscanf(mapping_list[i].network_addr, "%d:0x%x:0x%x", &slave, &index, &subindex);
		if(slave >= slave_count)
		{
			printf("EtherCAT cannot find slave %d! (max : %d)\n", slave, slave_count - 1);
			clear_inout_list();
			return 1;
		}

		pdo_entry_reg[i].alias = 0;
		pdo_entry_reg[i].position = slave;
		pdo_entry_reg[i].vendor_id = slave_info_list[slave].vendor_id;
		pdo_entry_reg[i].product_code = slave_info_list[slave].product_code;
		pdo_entry_reg[i].index = index;
		pdo_entry_reg[i].subindex = subindex;
		pdo_entry_reg[i].offset = &(temp_target -> offset);
		pdo_entry_reg[i].bit_position = &(temp_target -> bit_pos);

		temp_target -> bit_length = get_pdo_bit_length(slave, index, subindex, mapping_list[i].direction);
		if(temp_target -> bit_length == 0)
		{
			printf("EtherCAT getting bit length of (%x, %x) object failed!\n", index, subindex);
			clear_inout_list();
			return 1;
		}
		if((temp_target -> bit_length / 8) > mapping_list[i].size)
		{
			printf("EtherCAT not enough size of model variable.\n");
			clear_inout_list();
			return 1;
		}
	}

	memset(&(pdo_entry_reg[mapping_count]), 0, sizeof(ec_pdo_entry_reg_t));
	ret = ecrt_domain_reg_pdo_entry_list(domain1, pdo_entry_reg);
	if(ret != 0)
	{
		printf("EtherCAT PDO registration failed!\n");
		clear_inout_list();
		return ret;
	}

	return 0;
}

int igh_activate(unsigned long long interval)
{
	int ret;

	ret = ecrt_master_set_send_interval(master, interval);
	if(ret != 0)
	{
		printf("EtherCAT setting send interval failed!\n");
		return -ret;
	}

	ret = ecrt_master_activate(master);
	if(ret != 0)
	{
		printf("EtherCAT master activation failed!\n");
		return -ret;
	}

	domain1_pd = ecrt_domain_data(domain1);
	if(domain1_pd == NULL)
	{
		printf("EtherCAT mapping process data failed!\n");
		return 1;
	}

	return 0;
}

int igh_exchange(void)
{
	int i;

	ecrt_master_receive(master);
	ecrt_domain_process(domain1);

	for(i = 0; i < output_count; i++)
	{
		switch(output_list[i].bit_length)
		{
			case 1 :
				EC_WRITE_BIT(domain1_pd + output_list[i].offset, output_list[i].bit_pos, *((char*)output_list[i].variable));
				break;
			case 8 :
				EC_WRITE_U8(domain1_pd + output_list[i].offset, *((char*)output_list[i].variable));
				break;
			case 16 :
				EC_WRITE_U16(domain1_pd + output_list[i].offset, *((short*)output_list[i].variable));
				break;
			case 32 :
				EC_WRITE_U32(domain1_pd + output_list[i].offset, *((int*)output_list[i].variable));
				break;
		}
	}

	ecrt_domain_queue(domain1);
	ecrt_master_send(master);

	for(i = 0; i < input_count; i++)
	{
		switch(input_list[i].bit_length)
		{
			case 1 :
				*((char*)(input_list[i].variable)) = EC_READ_BIT(domain1_pd + input_list[i].offset, input_list[i].bit_pos);
				break;
			case 8 :
				*((char*)(input_list[i].variable)) = EC_READ_U8(domain1_pd + input_list[i].offset);
				break;
			case 16 :
				*((short*)(input_list[i].variable)) = EC_READ_U16(domain1_pd + input_list[i].offset);
				break;
			case 32 :
				*((int*)(input_list[i].variable)) = EC_READ_U32(domain1_pd + input_list[i].offset);
				break;
		}
	}

	return 0;
}

int igh_cleanup(igh_slave_t** slave_list)
{
	if(master != NULL)
	{
		ecrt_release_master(master);
		master = NULL;
	}

	if(slave_info_list != NULL)
	{
		free(slave_info_list);
		slave_info_list = NULL;
	}

	free_sync_info_list(input_sync_info_list);
	input_sync_info_list = NULL;
	free_sync_info_list(output_sync_info_list);
	output_sync_info_list = NULL;
	clear_inout_list();

	if(*slave_list != NULL)
	{
		free(*slave_list);
		*slave_list = NULL;
	}

	return 0;
}

static unsigned int get_pdo_bit_length(uint16_t slave, uint16_t index, uint8_t subindex, int direction)
{
	int i, j;
	ec_sync_info_t* temp_sync_info;

	if(direction == 1)
		temp_sync_info = &input_sync_info_list[slave];
	else if(direction == 0)
		temp_sync_info = &output_sync_info_list[slave];

	if(temp_sync_info -> pdos == NULL)
		return 0;

	for(i = 0; i < temp_sync_info -> n_pdos; i++)
	{
		if(temp_sync_info -> pdos[i].entries == NULL)
			continue;

		for(j = 0; j < temp_sync_info -> pdos[i].n_entries; j++)
		{
			if(index == temp_sync_info -> pdos[i].entries[j].index &&
				subindex == temp_sync_info -> pdos[i].entries[j].subindex)
				return temp_sync_info -> pdos[i].entries[j].bit_length;
		}
	}

	return 0;
}

static void free_sync_info_list(ec_sync_info_t* sync_info_list)
{
	int i, j;

	if(sync_info_list != NULL)
	{
		for(i = 0; i < slave_count; i++)
		{
			if(sync_info_list[i].pdos != NULL)
			{
				for(j = 0; j < sync_info_list[i].n_pdos; j++)
				{
					if(sync_info_list[i].pdos[j].entries != NULL)
					{
						free(sync_info_list[i].pdos[j].entries);
					}
				}
				free(sync_info_list[i].pdos);
			}
		}
		free(sync_info_list);
	}
}

static void clear_inout_list()
{
	if(pdo_entry_reg != NULL)
	{
		free(pdo_entry_reg);
		pdo_entry_reg = NULL;
	}

	if(input_list != NULL)
	{
		free(input_list);
		input_list = NULL;
		input_count = 0;
	}

	if(output_list != NULL)
	{
		free(output_list);
		output_list = NULL;
		output_count = 0;
	}
}
