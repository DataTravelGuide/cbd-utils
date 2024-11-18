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

static inline void transport_host_id_path(int transport_id, char *buffer, size_t buffer_size)
{
	snprintf(buffer, buffer_size, "%s%u/host_id", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

static inline void transport_adm_path(int transport_id, char *buffer, size_t buffer_size)
{
	/* Generate the path with transport_id */
	snprintf(buffer, buffer_size, "%s%u/adm", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

#define CBDSYS_PATH(OBJ, MEMBER)                                                                            \
static inline void OBJ##_##MEMBER##_path(unsigned int t_id, unsigned int obj_id, char *buffer, size_t buffer_size) \
{                                                                                                           \
        snprintf(buffer, buffer_size, "%s%u/cbd_" #OBJ "s/" #OBJ "%u/" #MEMBER, SYSFS_TRANSPORT_BASE_PATH, t_id, obj_id); \
}

CBDSYS_PATH(host, alive)
CBDSYS_PATH(host, hostname)

CBDSYS_PATH(blkdev, host_id)
CBDSYS_PATH(blkdev, backend_id)
CBDSYS_PATH(blkdev, alive)
CBDSYS_PATH(blkdev, mapped_id)

CBDSYS_PATH(backend, host_id)
CBDSYS_PATH(backend, path)
CBDSYS_PATH(backend, alive)
CBDSYS_PATH(backend, cache_segs)
CBDSYS_PATH(backend, gc_percent)

int cbdsys_backend_blkdevs_clear(struct cbd_transport *cbdt, unsigned int backend_id);

int cbdsys_transport_init(struct cbd_transport *cbdt, int transport_id);
int cbdsys_host_init(struct cbd_transport *cbdt, struct cbd_host *host, unsigned int host_id);
int cbdsys_blkdev_init(struct cbd_transport *cbdt, struct cbd_blkdev *blkdev, unsigned int blkdev_id);
int cbdsys_backend_init(struct cbd_transport *cbdt, struct cbd_backend *backend, unsigned int backend_id);
int cbdsys_find_backend_id_from_path(struct cbd_transport *cbdt, char *path, unsigned int *backend_id);
int cbdsys_write_value(const char *path, const char *value);

#endif // CBDSYS_H
