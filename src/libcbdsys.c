#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sysfs/libsysfs.h>

#include "cbdctrl.h"
#include "libcbdsys.h"

typedef int (*blkdev_cb_t)(unsigned int t_id, unsigned int blkdev_id);
typedef int (*blkdev_filter_t)(unsigned int t_id, unsigned int blkdev_id, void *data);

#define OBJ_CLEAN(OBJ, OBJ_NAME, CMD_PREFIX)						\
static int OBJ##_clean(unsigned int t_id, unsigned int id)				\
{											\
	char alive_path[FILE_NAME_SIZE];						\
	char adm_path[FILE_NAME_SIZE];							\
	char cmd[FILE_NAME_SIZE * 3] = { 0 };						\
	struct sysfs_attribute *sysattr;						\
	int ret;									\
											\
	/* Construct the path to the 'alive' file */					\
	OBJ##_alive_path(t_id, id, alive_path, sizeof(alive_path));			\
	/* Open and read the 'alive' status */						\
	sysattr = sysfs_open_attribute(alive_path);					\
	if (!sysattr) {									\
		printf("Failed to open '%s'\n", alive_path);				\
		return -1;								\
	}										\
	/* Read the attribute value */							\
	ret = sysfs_read_attribute(sysattr);						\
	if (ret < 0) {									\
		printf("Failed to read attribute '%s'\n", alive_path);			\
		sysfs_close_attribute(sysattr);						\
		return -1;								\
	}										\
	if (strncmp(sysattr->value, "true", 4) == 0) {					\
		sysfs_close_attribute(sysattr);						\
		return 0;								\
	}										\
	sysfs_close_attribute(sysattr);							\
											\
	/* Construct the command and admin path */					\
	snprintf(cmd, sizeof(cmd), "op="CMD_PREFIX"-clear,"CMD_PREFIX"_id=%u", id);	\
	transport_adm_path(t_id, adm_path, sizeof(adm_path));				\
	/* Open the admin interface and write the command */				\
	sysattr = sysfs_open_attribute(adm_path);					\
	if (!sysattr) {									\
		printf("Failed to open '%s'\n", adm_path);				\
		return -1;								\
	}										\
											\
	ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));				\
	sysfs_close_attribute(sysattr);							\
	if (ret != 0) {									\
		printf("Failed to write '%s'. Error: %s\n", cmd, strerror(ret));	\
	}										\
											\
	return ret;									\
}											\
											\
int OBJ##s_clean(unsigned int t_id) {							\
	char path[FILE_NAME_SIZE];							\
	DIR *dir;									\
	struct dirent *entry;								\
	unsigned int id;								\
	int ret;									\
											\
	transport_##OBJ##s_dir(t_id, path, sizeof(path));				\
	dir = opendir(path);								\
	if (!dir) {									\
		printf("Failed to open directory '%s': %s\n", path, strerror(errno));	\
		return -1;								\
	}										\
											\
	while ((entry = readdir(dir)) != NULL) {					\
		if (strncmp(entry->d_name, OBJ_NAME, strlen(OBJ_NAME)) == 0) {		\
			id = strtoul(entry->d_name + strlen(OBJ_NAME), NULL, 10);	\
			ret = OBJ##_clean(t_id, id);					\
			if (ret) {							\
				printf("failed to clean "OBJ_NAME"%u\n", id);		\
				return ret;						\
			}								\
		}									\
	}										\
											\
	closedir(dir);									\
	return 0;									\
}

OBJ_CLEAN(blkdev, "blkdev", "dev")
OBJ_CLEAN(backend, "backend", "backend")
OBJ_CLEAN(host, "host", "host")

/**
 * Iterate over each block device for the given transport ID and apply a callback if a filter condition is met.
 *
 * @param t_id          Transport ID
 * @param blkdev_cb     Callback function to invoke for each block device
 * @param blkdev_filter Filter function to determine if the callback should be applied
 * @param data          Data pointer passed to the filter function
 *
 * @return 0 on success, or an error code if a callback invocation fails
 */
static int for_each_blkdev(unsigned int t_id, blkdev_cb_t blkdev_cb, blkdev_filter_t blkdev_filter, void *data) {
	char path[FILE_NAME_SIZE];
	DIR *dir;
	struct dirent *entry;
	unsigned int blkdev_id;
	int ret;

	/* Retrieve the directory path for block devices for this transport ID */
	transport_blkdevs_dir(t_id, path, sizeof(path));
	dir = opendir(path);
	if (!dir) {
		printf("Failed to open directory '%s': %s\n", path, strerror(errno));
		return -1;
	}

	/* Iterate over each entry in the directory */
	while ((entry = readdir(dir)) != NULL) {
		/* Check if the entry name starts with "blkdev" */
		if (strncmp(entry->d_name, "blkdev", strlen("blkdev")) == 0) {
			/* Extract the ID from the entry name after "blkdev" prefix */
			blkdev_id = strtoul(entry->d_name + strlen("blkdev"), NULL, 10);

			/* Apply the filter function; if false, continue to next entry */
			if (blkdev_filter && !blkdev_filter(t_id, blkdev_id, data)) {
				continue;
			}

			/* Invoke the callback function on the block device ID */
			ret = blkdev_cb(t_id, blkdev_id);
			if (ret) {
				printf("Callback function failed for blkdev%u\n", blkdev_id);
				closedir(dir);
				return ret;
			}
		}
	}

	/* Close the directory after processing all entries */
	closedir(dir);
	return 0;
}

/**
 * Retrieve the backend ID for a block device.
 *
 * @param t_id          Transport ID
 * @param blkdev_id     Block device ID
 * @param backend_id    Pointer to store the resulting backend ID
 *
 * @return 0 on success, -EINVAL if the value is empty, or an error code on failure
 */
static int blkdev_backend_id(unsigned int t_id, unsigned int blkdev_id, unsigned int *backend_id) {
	char path[FILE_NAME_SIZE];
	struct sysfs_attribute *sysattr;
	int ret;

	/* Construct the path to the backend ID file */
	blkdev_backend_id_path(t_id, blkdev_id, path, sizeof(path));

	/* Open and read the backend ID attribute */
	sysattr = sysfs_open_attribute(path);
	if (!sysattr) {
		printf("Failed to open '%s'\n", path);
		return -errno;
	}

	/* Read the attribute value */
	ret = sysfs_read_attribute(sysattr);
	if (ret < 0) {
		printf("Failed to read attribute '%s'\n", path);
		sysfs_close_attribute(sysattr);
		return ret;
	}

	/* Check if the value is empty */
	if (sysattr->value[0] == '\0') {
		sysfs_close_attribute(sysattr);
		return -EINVAL;
	}

	/* Convert value to unsigned int and store it in backend_id */
	*backend_id = (unsigned int)strtoul(sysattr->value, NULL, 10);

	sysfs_close_attribute(sysattr);
	return 0;
}


/* Filter function to match backend_id with specified data */
static int backend_filter(unsigned int t_id, unsigned int blkdev_id, void *data) {
	unsigned int target_backend_id;
	unsigned int filter_backend_id = *(unsigned int *)data;

	/* Get the backend ID of the current blkdev */
	if (blkdev_backend_id(t_id, blkdev_id, &target_backend_id) != 0) {
		return false; /* Return false on failure */
	}

	/* Check if backend IDs match */
	return target_backend_id == filter_backend_id;
}

/* Callback function to clean the block device */
static int blkdev_cb_clean(unsigned int t_id, unsigned int blkdev_id) {
	return blkdev_clean(t_id, blkdev_id);
}

/* Main function to clear block devices associated with a specific backend */
int backend_blkdevs_clear(unsigned int t_id, unsigned int backend_id) {
	/* Use for_each_blkdev with the filter and callback */
	return for_each_blkdev(t_id, blkdev_cb_clean, backend_filter, &backend_id);
}

