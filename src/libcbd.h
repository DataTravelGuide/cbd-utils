#ifndef CBD_H
#define CBD_H

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
};

#endif // CBD_H
