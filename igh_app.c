#include "io.h"

#include "ecrt.h"
#include "igh.h"
#include "cia402.h"

static igh_slave_t* slave_list = NULL;
static int slave_count = 0;

static cia402_node_t* cia402_node_list = NULL;
static int cia402_node_count = 0;

int io_init(void)
{
	int ret;

	ret = igh_init(&slave_list, &slave_count);
	if(ret != 0)
		return ret;

	ret = cia402_get_node_list(slave_list, slave_count, &cia402_node_list, &cia402_node_count);
	if(ret != 0)
		return ret;

	return 0;
}

int io_mapping(io_mapping_info_t* mapping_list, int mapping_count)
{
	int ret = 0;
	io_mapping_info_t* cia402_mapping_list;
	int cia402_mapping_count = 0;

	ret = cia402_get_mapping_list(cia402_node_list, cia402_node_count, mapping_list, mapping_count,
		&cia402_mapping_list, &cia402_mapping_count);
	if(ret != 0)
		return ret;

	ret = igh_mapping(cia402_mapping_list, cia402_mapping_count);
	cia402_free_mapping_list(&cia402_mapping_list);

	return ret;
}

int io_activate(unsigned long long interval)
{
	return igh_activate(interval);
}

int io_exchange(void)
{
	int ret;

	cia402_publish(cia402_node_list, cia402_node_count);

	ret = igh_exchange();
	if(ret != 0)
		return ret;

	cia402_retrieve(cia402_node_list, cia402_node_count);

	return 0;
}

int io_cleanup(void)
{
	cia402_free_node_list(&cia402_node_list);
	return igh_cleanup(&slave_list);
}
