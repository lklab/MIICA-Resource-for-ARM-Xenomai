#include "cia402.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecrt.h"
#include "igh.h"
#include "io.h"

#define CW_INDEX 0x6040

#define PW_CTL_NAME "power"
#define PW_FDB_NAME "enabled"
#define POS_TGT_NAME "target"
#define POS_FCT_NAME "factor"

/* From CiA402, page 27

	copied from plc_cia402node.c in beremiz (180119)

		Table 30 - State coding
	Statusword      |      PDS FSA state
xxxx xxxx x0xx 0000 | Not ready to switch on
xxxx xxxx x1xx 0000 | Switch on disabled
xxxx xxxx x01x 0001 | Ready to switch on
xxxx xxxx x01x 0011 | Switched on
xxxx xxxx x01x 0111 | Operation enabled
xxxx xxxx x00x 0111 | Quick stop active
xxxx xxxx x0xx 1111 | Fault reaction active
xxxx xxxx x0xx 1000 | Fault
*/
#define FSAFromStatusWord(SW) (SW & 0x006f)
#define NotReadyToSwitchOn   0b00000000
#define NotReadyToSwitchOn2  0b00100000
#define SwitchOnDisabled     0b01000000
#define SwitchOnDisabled2    0b01100000
#define ReadyToSwitchOn      0b00100001
#define SwitchedOn           0b00100011
#define OperationEnabled     0b00100111
#define QuickStopActive      0b00000111
#define FaultReactionActive  0b00001111
#define FaultReactionActive2 0b00101111
#define Fault                0b00001000
#define Fault2               0b00101000

// SatusWord bits :
#define SW_ReadyToSwitchOn     0x0001
#define SW_SwitchedOn          0x0002
#define SW_OperationEnabled    0x0004
#define SW_Fault               0x0008
#define SW_VoltageEnabled      0x0010
#define SW_QuickStop           0x0020
#define SW_SwitchOnDisabled    0x0040
#define SW_Warning             0x0080
#define SW_Remote              0x0200
#define SW_TargetReached       0x0400
#define SW_InternalLimitActive 0x0800

// ControlWord bits :
#define SwitchOn        0x0001
#define EnableVoltage   0x0002
#define QuickStop       0x0004
#define EnableOperation 0x0008
#define FaultReset      0x0080
#define Halt            0x0100
/* CiA402 statemachine definition end */

static int is_cia402_node(igh_slave_t* slave);
static int get_index_from_position(cia402_node_t* cia402_node_list, int cia402_node_count, int position);

int cia402_get_node_list(igh_slave_t* slave_list, int slave_count,
	cia402_node_t** cia402_node_list, int* cia402_node_count)
{
	int i, j;
	int node_count = 0;

	*cia402_node_count = 0;
	*cia402_node_list = NULL;

	/* count CiA402 nodes */
	for(i = 0; i < slave_count; i++)
	{
		if(is_cia402_node(&slave_list[i]))
			node_count++;
	}

	*cia402_node_count = node_count;
	*cia402_node_list = (cia402_node_t*)malloc(sizeof(cia402_node_t) * node_count);
	memset(*cia402_node_list, 0, sizeof(cia402_node_t) * node_count);

	/* initialize CiA402 nodes */
	j = 0;
	for(i = 0; i < slave_count; i++)
	{
		if(is_cia402_node(&slave_list[i]))
		{
			(*cia402_node_list)[j].position = i;
			(*cia402_node_list)[j].mode_of_operation = 0x8;
			sprintf((*cia402_node_list)[j].cw_address, "%d:0x6040:0x0", i);
			sprintf((*cia402_node_list)[j].sw_address, "%d:0x6041:0x0", i);
			sprintf((*cia402_node_list)[j].mo_address, "%d:0x6060:0x0", i);
			sprintf((*cia402_node_list)[j].tp_address, "%d:0x607a:0x0", i);
			j++;
		}
	}

	return 0;
}

int cia402_free_node_list(cia402_node_t** cia402_node_list)
{
	if(*cia402_node_list != NULL)
	{
		free(*cia402_node_list);
		*cia402_node_list = NULL;
	}

	return 0;
}

int cia402_get_mapping_list(cia402_node_t* cia402_node_list, int cia402_node_count,
	io_mapping_info_t* mapping_list, int mapping_count,
	io_mapping_info_t** cia402_mapping_list, int* cia402_mapping_count)
{
	int i, j, k, index;
	int position;
	char buffer[1023];

	*cia402_mapping_count = cia402_node_count + mapping_count;
	*cia402_mapping_list = (io_mapping_info_t*)malloc(sizeof(io_mapping_info_t) * (cia402_node_count + mapping_count));

	for(i = 0, k = 0; i < mapping_count; i++, k++)
	{
		sscanf(mapping_list[i].network_addr, "%d:%s", &position, buffer);

		/* power control mapping info is changed to control word */
		if(!strcmp(buffer, PW_CTL_NAME))
		{
			index = get_index_from_position(cia402_node_list, cia402_node_count, position);
			if(index == -1)
			{
				printf("EtherCAT %d slave is not CiA402 node!\n", position);
				cia402_free_mapping_list(cia402_mapping_list);
				return 1;
			}

			cia402_node_list[index].power_control = (int*)mapping_list[i].model_addr;
			(*cia402_mapping_list)[k].model_addr = &(cia402_node_list[index].control_word);
			(*cia402_mapping_list)[k].size = sizeof(int);
			(*cia402_mapping_list)[k].network_addr = cia402_node_list[index].cw_address;
			(*cia402_mapping_list)[k].direction = 0;
		}
		/* power feedback mapping info is changed to status word */
		else if(!strcmp(buffer, PW_FDB_NAME))
		{
			index = get_index_from_position(cia402_node_list, cia402_node_count, position);
			if(index == -1)
			{
				printf("EtherCAT %d slave is not CiA402 node!\n", position);
				cia402_free_mapping_list(cia402_mapping_list);
				return 1;
			}

			cia402_node_list[index].power_feedback = (int*)mapping_list[i].model_addr;
			(*cia402_mapping_list)[k].model_addr = &(cia402_node_list[index].status_word);
			(*cia402_mapping_list)[k].size = sizeof(int);
			(*cia402_mapping_list)[k].network_addr = cia402_node_list[index].sw_address;
			(*cia402_mapping_list)[k].direction = 1;
		}
		/* scaled target position info is changed to raw target position */
		else if(!strcmp(buffer, POS_TGT_NAME))
		{
			index = get_index_from_position(cia402_node_list, cia402_node_count, position);
			if(index == -1)
			{
				printf("EtherCAT %d slave is not CiA402 node!\n", position);
				cia402_free_mapping_list(cia402_mapping_list);
				return 1;
			}

			cia402_node_list[index].scaled_target = (int*)mapping_list[i].model_addr;
			(*cia402_mapping_list)[k].model_addr = &(cia402_node_list[index].target_position);
			(*cia402_mapping_list)[k].size = sizeof(int);
			(*cia402_mapping_list)[k].network_addr = cia402_node_list[index].tp_address;
			(*cia402_mapping_list)[k].direction = 0;
		}
		/* target position scale factor info is stored to CiA402 structure */
		else if(!strcmp(buffer, POS_FCT_NAME))
		{
			index = get_index_from_position(cia402_node_list, cia402_node_count, position);
			if(index == -1)
			{
				printf("EtherCAT %d slave is not CiA402 node!\n", position);
				cia402_free_mapping_list(cia402_mapping_list);
				return 1;
			}

			cia402_node_list[index].scale_factor = (double*)mapping_list[i].model_addr;
			(*cia402_mapping_count)--;
			k--;
		}
		/* other mapping info is copied as it is */
		else
			memcpy(&((*cia402_mapping_list)[i]), &(mapping_list[i]), sizeof(io_mapping_info_t));
	}

	/* setting up mapping infomation to mode of operation */
	for(j = 0; j < cia402_node_count; j++, k++)
	{
		(*cia402_mapping_list)[k].model_addr = &(cia402_node_list[j].mode_of_operation);
		(*cia402_mapping_list)[k].size = sizeof(int);
		(*cia402_mapping_list)[k].network_addr = cia402_node_list[j].mo_address;
		(*cia402_mapping_list)[k].direction = 0;
	}

	return 0;
}

int cia402_free_mapping_list(io_mapping_info_t** cia402_mapping_list)
{
	if(*cia402_mapping_list != NULL)
	{
		free(*cia402_mapping_list);
		*cia402_mapping_list = NULL;
	}

	return 0;
}

int cia402_publish(cia402_node_t* cia402_node_list, int cia402_node_count)
{
	int i;
	int power, cw;

	for(i = 0; i < cia402_node_count; i++)
	{
		// power command
		if(cia402_node_list[i].power_control == NULL)
			power = 0;
		else
		{
			power = (cia402_node_list[i].status_word & SW_VoltageEnabled)
				&& *(cia402_node_list[i].power_control);
		}
		cw = cia402_node_list[i].control_word;

		// CiA402 statemachine (copied from plc_cia402node.c in beremiz (180119))
		switch(FSAFromStatusWord(cia402_node_list[i].status_word))
		{
			case SwitchOnDisabled :
			case SwitchOnDisabled2 :
				cw &= ~(SwitchOn | FaultReset);
				cw |= EnableVoltage | QuickStop;
				break;
			case ReadyToSwitchOn :
			case OperationEnabled :
				if(!power)
				{
					cw &= ~(FaultReset | EnableOperation);
					cw |= SwitchOn | EnableVoltage | QuickStop;
					break;
				}
			case SwitchedOn :
				if(power)
				{
					cw &= ~(FaultReset);
					cw |= SwitchOn | EnableVoltage | QuickStop | EnableOperation;
				}
				break;
			case Fault :
			case Fault2 :
				cw &= ~(SwitchOn | EnableVoltage | QuickStop | EnableOperation);
				cw |= FaultReset;
				break;
			default:
				break;
		}
		cia402_node_list[i].control_word = cw;

		// cacluate raw target position
		if(cia402_node_list[i].scaled_target != NULL)
		{
			if(cia402_node_list[i].scale_factor == NULL)
				cia402_node_list[i].target_position = *(cia402_node_list[i].scaled_target);
			else
				cia402_node_list[i].target_position =
					(int)((double)*(cia402_node_list[i].scaled_target) * *(cia402_node_list[i].scale_factor));
		}
	}

	return 0;
}

int cia402_retrieve(cia402_node_t* cia402_node_list, int cia402_node_count)
{
	int i;

	for(i = 0; i < cia402_node_count; i++)
	{
		if(cia402_node_list[i].power_feedback == NULL)
			continue;

		int fsa = FSAFromStatusWord(cia402_node_list[i].status_word);
		*(cia402_node_list[i].power_feedback) = fsa == OperationEnabled;
	}

	return 0;
}

static int is_cia402_node(igh_slave_t* slave)
{
	int i, j;
	ec_sync_info_t* sync_info = slave -> output_sync_info_p;

	if(sync_info -> pdos == NULL)
		return 0;

	for(i = 0; i < sync_info -> n_pdos; i++)
	{
		if(sync_info -> pdos[i].entries == NULL)
			continue;

		for(j = 0; j < sync_info -> pdos[i].n_entries; j++)
		{
			/* check whether control word is exists */
			if(sync_info -> pdos[i].entries[j].index == CW_INDEX)
				return 1;
		}
	}

	return 0;
}

static int get_index_from_position(cia402_node_t* cia402_node_list, int cia402_node_count, int position)
{
	int i;

	for(i = 0; i < cia402_node_count; i++)
	{
		if(cia402_node_list[i].position == position)
			return i;
	}

	return -1;
}
