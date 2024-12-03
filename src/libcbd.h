#ifndef CBD_H
#define CBD_H

#define CBD_NAME_LEN            32

struct cbd_transport {
	uint64_t magic;
	int version;
	int flags;
	unsigned int host_area_off;
	unsigned int bytes_per_host_info;
	unsigned int host_num;
	unsigned int backend_area_off;
	unsigned int bytes_per_backend_info;
	unsigned int backend_num;
	unsigned int blkdev_area_off;
	unsigned int bytes_per_blkdev_info;
	unsigned int blkdev_num;
	unsigned int segment_area_off;
	unsigned int bytes_per_segment;
	unsigned int segment_num;
	char path[CBD_PATH_LEN];
	unsigned int transport_id;
	unsigned int host_id;
};

struct cbd_host {
	int host_id;
	char hostname[CBD_NAME_LEN];
	bool alive;
};

struct cbd_blkdev {
	unsigned int blkdev_id;
	unsigned int host_id;
	unsigned int backend_id;
	char dev_name[CBD_NAME_LEN];
	bool alive;
};

#define CBDB_BLKDEV_COUNT_MAX   1

struct cbd_backend {
	unsigned int backend_id;
	unsigned int host_id;
	char backend_path[CBD_PATH_LEN];
	bool alive;
	unsigned int cache_segs;
	unsigned int cache_gc_percent;
	unsigned int cache_used_segs;
	unsigned int dev_num;
	struct cbd_blkdev blkdevs[CBDB_BLKDEV_COUNT_MAX];
};

#endif // CBD_H
