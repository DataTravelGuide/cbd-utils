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

#define OBJ_CLEAN(OBJ, OBJ_NAME, CMD_PREFIX)						\
static int OBJ##_clean(unsigned int t_id, unsigned int id)				\
{											\
	char alive_path[CBD_PATH_LEN];						\
	char adm_path[CBD_PATH_LEN];							\
	char cmd[CBD_PATH_LEN * 3] = { 0 };						\
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
	char path[CBD_PATH_LEN];							\
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

typedef int (*cbdsys_cb_t)(unsigned int t_id, unsigned int blkdev_id, void *ctx);
typedef int (*cbdsys_filter_t)(unsigned int t_id, unsigned int blkdev_id, void *ctx);

struct cbdsys_walk_ctx {
	cbdsys_cb_t	cb;
	cbdsys_filter_t	filter;
	void		*data;
};

struct cbdsys_blkdev_walk_data {
	unsigned int backend_id;
};

/* Macro to define object-specific iteration functions */
#define OBJ_WALK(OBJ)                                                             \
    static int for_each_##OBJ(unsigned int t_id, struct cbdsys_walk_ctx *walk_ctx) { \
        char path[CBD_PATH_LEN];                                                \
        DIR *dir;                                                                 \
        struct dirent *entry;                                                     \
        unsigned int obj_id;                                                      \
        int ret;                                                                  \
                                                                                  \
        /* Retrieve the directory path for the object */                          \
        transport_##OBJ##s_dir(t_id, path, sizeof(path));                         \
        dir = opendir(path);                                                      \
        if (!dir) {                                                               \
            printf("Failed to open directory '%s': %s\n", path, strerror(errno)); \
            return -1;                                                            \
        }                                                                         \
                                                                                  \
        /* Iterate over each entry in the directory */                            \
        while ((entry = readdir(dir)) != NULL) {                                  \
            /* Check if the entry name starts with OBJ */                         \
            if (strncmp(entry->d_name, #OBJ, strlen(#OBJ)) == 0) {                \
                /* Extract the ID from the entry name after OBJ prefix */         \
                obj_id = strtoul(entry->d_name + strlen(#OBJ), NULL, 10);         \
                                                                                  \
                /* Apply the filter function; if false, continue to next entry */ \
                if (walk_ctx->filter && !walk_ctx->filter(t_id, obj_id, walk_ctx->data)) { \
                    continue;                                                     \
                }                                                                 \
                                                                                  \
                /* Invoke the callback function on the object ID */               \
                ret = walk_ctx->cb(t_id, obj_id, walk_ctx->data);                 \
                if (ret) {                                                        \
                    printf("Callback function failed for %s%u\n", #OBJ, obj_id);  \
                    closedir(dir);                                                \
                    return ret;                                                   \
                }                                                                 \
            }                                                                     \
        }                                                                         \
                                                                                  \
        /* Close the directory after processing all entries */                    \
        closedir(dir);                                                            \
        return 0;                                                                 \
    }

/* Example usage of the macro */
OBJ_WALK(blkdev)
OBJ_WALK(backend)
OBJ_WALK(host)

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
	char path[CBD_PATH_LEN];
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
static int blkdev_filter_backend(unsigned int t_id, unsigned int blkdev_id, void *data) {
	unsigned int target_backend_id;
	struct cbdsys_blkdev_walk_data *ctx = data;

	if (ctx->backend_id == UINT_MAX)
		return true;

	/* Get the backend ID of the current blkdev */
	if (blkdev_backend_id(t_id, blkdev_id, &target_backend_id) != 0) {
		return false; /* Return false on failure */
	}

	/* Check if backend IDs match */
	return target_backend_id == ctx->backend_id;
}

/* Callback function to clean the block device */
static int blkdev_cb_clear(unsigned int t_id, unsigned int blkdev_id, void *data) {
	return blkdev_clean(t_id, blkdev_id);
}

/* Main function to clear block devices associated with a specific backend */
int backend_blkdevs_clear(unsigned int t_id, unsigned int backend_id) {
	struct cbdsys_blkdev_walk_data data = { 0 };
	struct cbdsys_walk_ctx ctx = { 0 };

	data.backend_id = backend_id;

	ctx.filter = blkdev_filter_backend;
	ctx.cb = blkdev_cb_clear;
	ctx.data = &data;

	return for_each_blkdev(t_id, &ctx);
}

int cbdsys_transport_init(struct cbd_transport *cbdt, int transport_id) {
	char path[CBD_PATH_LEN];
	FILE *file;
	char attribute[64];
	uint64_t value;
	int ret;

	/* Construct the file path */
	transport_info_path(transport_id, path, CBD_PATH_LEN);

	/* Open the file */
	file = fopen(path, "r");
	if (!file) {
		printf("failed to open %s\n", path);
		return -errno;  // Return error code
	}

	/* Read and parse each line */
	while (fscanf(file, "%63[^:]: %lx\n", attribute, &value) == 2) {
		if (strcmp(attribute, "magic") == 0) {
			cbdt->magic = (typeof(cbdt->magic))value;
		} else if (strcmp(attribute, "version") == 0) {
			cbdt->version = (typeof(cbdt->version))value;
		} else if (strcmp(attribute, "flags") == 0) {
			cbdt->flags = (typeof(cbdt->flags))value;
		} else if (strcmp(attribute, "host_area_off") == 0) {
			cbdt->host_area_off = (typeof(cbdt->host_area_off))value;
		} else if (strcmp(attribute, "bytes_per_host_info") == 0) {
			cbdt->bytes_per_host_info = (typeof(cbdt->bytes_per_host_info))value;
		} else if (strcmp(attribute, "host_num") == 0) {
			cbdt->host_num = (typeof(cbdt->host_num))value;
		} else if (strcmp(attribute, "backend_area_off") == 0) {
			cbdt->backend_area_off = (typeof(cbdt->backend_area_off))value;
		} else if (strcmp(attribute, "bytes_per_backend_info") == 0) {
			cbdt->bytes_per_backend_info = (typeof(cbdt->bytes_per_backend_info))value;
		} else if (strcmp(attribute, "backend_num") == 0) {
			cbdt->backend_num = (typeof(cbdt->backend_num))value;
		} else if (strcmp(attribute, "blkdev_area_off") == 0) {
			cbdt->blkdev_area_off = (typeof(cbdt->blkdev_area_off))value;
		} else if (strcmp(attribute, "bytes_per_blkdev_info") == 0) {
			cbdt->bytes_per_blkdev_info = (typeof(cbdt->bytes_per_blkdev_info))value;
		} else if (strcmp(attribute, "blkdev_num") == 0) {
			cbdt->blkdev_num = (typeof(cbdt->blkdev_num))value;
		} else if (strcmp(attribute, "segment_area_off") == 0) {
			cbdt->segment_area_off = (typeof(cbdt->segment_area_off))value;
		} else if (strcmp(attribute, "bytes_per_segment") == 0) {
			cbdt->bytes_per_segment = (typeof(cbdt->bytes_per_segment))value;
		} else if (strcmp(attribute, "segment_num") == 0) {
			cbdt->segment_num = (typeof(cbdt->segment_num))value;
		} else {
			/* Unrecognized attribute, ignore */
		}
	}
	/* Close the file */
	fclose(file);

	transport_path_path(transport_id, path, CBD_PATH_LEN);

	/* Open the file for reading */
	file = fopen(path, "r");
	if (file == NULL) {
		perror("Error opening file");
		return -errno;
	}

	/* Read content from file into cbdt->path */
	if (fgets(cbdt->path, CBD_PATH_LEN, file) == NULL) {
		if (ferror(file)) {
			perror("Error reading file");
			fclose(file);
			return -errno;
		}
	}

	/* Close the file */
	fclose(file);

	/* Ensure null-termination of the string in cbdt->path */
	cbdt->path[CBD_PATH_LEN - 1] = '\0';

	return 0;
}
