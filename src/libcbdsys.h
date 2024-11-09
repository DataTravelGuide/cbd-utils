#ifndef CBDSYS_H
#define CBDSYS_H

#include <stdint.h>

#include "libcbd.h"

#define SYSFS_CBD_TRANSPORT_REGISTER "/sys/bus/cbd/transport_register"
#define SYSFS_CBD_TRANSPORT_UNREGISTER "/sys/bus/cbd/transport_unregister"
#define SYSFS_TRANSPORT_BASE_PATH "/sys/bus/cbd/devices/transport"

static inline void transport_info_path(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/info", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void transport_path_path(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/path", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void transport_adm_path(int transport_id, char *buffer, size_t buffer_size)
{
	/* Generate the path with transport_id */
	snprintf(buffer, buffer_size, "%s%u/adm", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void transport_hosts_dir(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_hosts/", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void host_alive_path(int transport_id, int host_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_hosts/host%u/alive", SYSFS_TRANSPORT_BASE_PATH, transport_id, host_id);
}

static inline void transport_blkdevs_dir(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_blkdevs/", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void blkdev_alive_path(int transport_id, int dev_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_blkdevs/blkdev%u/alive", SYSFS_TRANSPORT_BASE_PATH, transport_id, dev_id);
}

static inline void blkdev_backend_id_path(int transport_id, int dev_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_blkdevs/blkdev%u/backend_id", SYSFS_TRANSPORT_BASE_PATH, transport_id, dev_id);
}

static inline void transport_backends_dir(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_backends/", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void backend_alive_path(int transport_id, int backend_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/cbd_backends/backend%u/alive", SYSFS_TRANSPORT_BASE_PATH, transport_id, backend_id);
}

int backend_blkdevs_clear(unsigned int t_id, unsigned int backend_id);

int cbdsys_transport_init(struct cbd_transport *cbdt, int transport_id);

#endif // CBDSYS_H
