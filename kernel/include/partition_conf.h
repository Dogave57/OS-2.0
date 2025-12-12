#ifndef _PARTITION_CONF
#define _PARTITION_CONF
struct partition_conf{
	uint64_t rootPartitionId;
	struct gpt_partition rootPartition;
}__attribute__((packed));
#endif
