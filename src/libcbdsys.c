#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sysfs/libsysfs.h>

#include "cbdctrl.h"
#include "libcbdsys.h"

static int blkdev_clean(unsigned int t_id, unsigned int blkdev_id)
{
        char alive_path[CBD_PATH_LEN];
        char adm_path[CBD_PATH_LEN];
        char cmd[CBD_PATH_LEN * 3] = { 0 };
        struct sysfs_attribute *sysattr;
        int ret;

        /* Construct the path to the 'alive' file for blkdev */
        blkdev_alive_path(t_id, blkdev_id, alive_path, sizeof(alive_path));

        /* Open and read the 'alive' status */
        sysattr = sysfs_open_attribute(alive_path);
        if (!sysattr) {
                printf("Failed to open '%s'\n", alive_path);
                return -1;
        }

        /* Read the attribute value */
        ret = sysfs_read_attribute(sysattr);
        if (ret < 0) {
                printf("Failed to read attribute '%s'\n", alive_path);
                sysfs_close_attribute(sysattr);
                return -1;
        }

        if (strncmp(sysattr->value, "true", 4) == 0) {
                sysfs_close_attribute(sysattr);
                return 0;
        }
        sysfs_close_attribute(sysattr);

        /* Construct the command and admin path */
        snprintf(cmd, sizeof(cmd), "op=dev-clear,dev_id=%u", blkdev_id);
        transport_adm_path(t_id, adm_path, sizeof(adm_path));

        /* Open the admin interface and write the command */
        sysattr = sysfs_open_attribute(adm_path);
        if (!sysattr) {
                printf("Failed to open '%s'\n", adm_path);
                return -1;
        }

        ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));
        sysfs_close_attribute(sysattr);
        if (ret != 0) {
                printf("Failed to write '%s'. Error: %s\n", cmd, strerror(ret));
        }

        return ret;
}

int cbdsys_backend_blkdevs_clear(struct cbd_transport *cbdt, unsigned int backend_id)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < cbdt->blkdev_num; i++) {
		struct cbd_blkdev blkdev;

		// Initialize the blkdev structure
		ret = cbdsys_blkdev_init(cbdt, &blkdev, i);
		if (ret < 0) {
			continue; // Skip if initialization fails
		}

		// Check if blkdev's backend_id matches the given backend_id
		if (blkdev.backend_id == backend_id) {
			ret = blkdev_clean(cbdt->transport_id, i);
			if (ret < 0) {
				printf("Failed to clear blkdev %u\n", i);
				return ret;
			}
		}
	}

	return 0;
}

int cbdsys_transport_init(struct cbd_transport *cbdt, int transport_id) {
	char path[CBD_PATH_LEN];
	FILE *file;
	char attribute[64];
	char value_str[64];;
	uint64_t value;
	int ret;

	cbdt->transport_id = transport_id;
	/* Construct the file path */
	transport_info_path(transport_id, path, CBD_PATH_LEN);

	/* Open the file */
	file = fopen(path, "r");
	if (!file) {
		//printf("failed to open %s\n", path);
		return -errno;  // Return error code
	}

	/* Read and parse each line */
	while (fscanf(file, "%63[^:]: %63s\n", attribute, value_str) == 2) {
		/* Check if the value is in hexadecimal by looking for "0x" prefix */
		if (strncmp(value_str, "0x", 2) == 0) {
			sscanf(value_str, "%lx", &value);
		} else {
			sscanf(value_str, "%lu", &value);
		}

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



	transport_host_id_path(transport_id, path, CBD_PATH_LEN);

	/* Open the file for reading */
	file = fopen(path, "r");
	if (file == NULL) {
		perror("Error opening file");
		return -errno;
	}

	/* Read unsigned int from the file and set cbdt->host_id */
	if (fscanf(file, "%u", &cbdt->host_id) != 1) {
		fprintf(stderr, "Error reading host_id from file: %s\n", path);
		fclose(file);
		return -EINVAL;
	}

	/* Close the file */
	fclose(file);

	return 0;
}

int cbdsys_host_init(struct cbd_transport *cbdt, struct cbd_host *host, unsigned int host_id)
{
	char path[CBD_PATH_LEN];
	FILE *file;

	// Initialize host_id
	host->host_id = host_id;

	// Read hostname
	host_hostname_path(cbdt->transport_id, host_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (file == NULL) {
		perror("Error opening hostname file");
		return -1;
	}
	if (fgets(host->hostname, sizeof(host->hostname), file) == NULL) {
		fclose(file);
		return -1;
	}
	// Remove newline character if present
	host->hostname[strcspn(host->hostname, "\n")] = '\0';
	fclose(file);

	// Read alive status
	host_alive_path(cbdt->transport_id, host_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (file == NULL) {
		perror("Error opening alive file");
		return -1;
	}
	char alive_str[8];
	if (fgets(alive_str, sizeof(alive_str), file) == NULL) {
		perror("Error reading alive status");
		fclose(file);
		return -1;
	}
	fclose(file);

	// Convert alive status string to boolean
	host->alive = (strcmp(alive_str, "true\n") == 0);

	return 0;
}

#define CBD_DEV_NAME_FORMAT "/dev/cbd%u"

int cbdsys_blkdev_init(struct cbd_transport *cbdt, struct cbd_blkdev *blkdev, unsigned int blkdev_id) {
	char path[CBD_PATH_LEN];
	FILE *file;
	unsigned int mapped_id;
	char buffer[16];

	// Load blkdev_id
	blkdev->blkdev_id = blkdev_id;

	// Load host_id
	blkdev_host_id_path(cbdt->transport_id, blkdev_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (!file) {
		perror("Error opening host_id");
		return -ENOENT;
	}
	if (fscanf(file, "%u", &blkdev->host_id) != 1) {
		fclose(file);
		return -ENOENT;
	}
	fclose(file);

	// Load backend_id
	blkdev_backend_id_path(cbdt->transport_id, blkdev_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (!file) {
		perror("Error opening backend_id");
		return -ENOENT;
	}
	if (fscanf(file, "%u", &blkdev->backend_id) != 1) {
		fclose(file);
		return -ENOENT;
	}
	fclose(file);

	// Load alive status
	blkdev_alive_path(cbdt->transport_id, blkdev_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (!file) {
		perror("Error opening alive");
		return -ENOENT;
	}
	if (fgets(buffer, sizeof(buffer), file)) {
		// Trim newline if present
		buffer[strcspn(buffer, "\n")] = '\0';
		blkdev->alive = (strcmp(buffer, "true") == 0);
	}
	fclose(file);

	// Load mapped_id and set dev_name
	blkdev_mapped_id_path(cbdt->transport_id, blkdev_id, path, CBD_PATH_LEN);
	file = fopen(path, "r");
	if (!file) {
		perror("Error opening mapped_id");
		return -ENOENT;
	}
	if (fscanf(file, "%u", &mapped_id) != 1) {
		fclose(file);
		return -ENOENT;
	}
	fclose(file);
	snprintf(blkdev->dev_name, sizeof(blkdev->dev_name), CBD_DEV_NAME_FORMAT, mapped_id);

	return 0;
}

int read_sysfs_value(const char *path, char *buf, size_t buf_len)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;

	if (fgets(buf, buf_len, f) == NULL) {
		fclose(f);
		return -1;
	}

	// Remove newline if present
	buf[strcspn(buf, "\n")] = '\0';
	fclose(f);
	return 0;
}

int cbdsys_backend_init(struct cbd_transport *cbdt, struct cbd_backend *backend, unsigned int backend_id)
{
	char path[CBD_PATH_LEN];
	char buf[CBD_PATH_LEN];
	struct stat sb;
	int ret;

	// Initialize backend_id
	backend->backend_id = backend_id;

	// Read host_id
	backend_host_id_path(cbdt->transport_id, backend_id, path, CBD_PATH_LEN);
	ret = read_sysfs_value(path, buf, sizeof(buf));
	if (ret < 0 || buf[0] == '\0') {
		return -ENOENT; // Return if host_id is empty
	}
	backend->host_id = atoi(buf);

	// Read backend_path
	backend_path_path(cbdt->transport_id, backend_id, path, CBD_PATH_LEN);
	ret = read_sysfs_value(path, backend->backend_path, sizeof(backend->backend_path));
	if (ret < 0) {
		return ret;
	}

	// Read alive status
	backend_alive_path(cbdt->transport_id, backend_id, path, CBD_PATH_LEN);
	ret = read_sysfs_value(path, buf, sizeof(buf));
	if (ret < 0) {
		return ret;
	}
	backend->alive = (strcmp(buf, "true") == 0);

	// Read cache_segs directly from the new path
	backend_cache_segs_path(cbdt->transport_id, backend_id, path, CBD_PATH_LEN);
	ret = read_sysfs_value(path, buf, sizeof(buf));
	if (ret < 0) {
		return ret;
	}
	backend->cache_segs = (unsigned int)atoi(buf);

	// Read gc_percent directly from the new path
	backend_gc_percent_path(cbdt->transport_id, backend_id, path, CBD_PATH_LEN);
	ret = read_sysfs_value(path, buf, sizeof(buf));
	if (ret < 0) {
		return ret;
	}
	backend->gc_percent = (unsigned int)atoi(buf);

	// Initialize block devices
	backend->dev_num = 0;
	for (unsigned int i = 0; i < cbdt->blkdev_num; i++) {
		struct cbd_blkdev blkdev;
		ret = cbdsys_blkdev_init(cbdt, &blkdev, i);
		if (ret < 0) {
			continue;
		}

		// Check if blkdev's backend_id matches the current backend_id
		if (blkdev.backend_id == backend_id) {
			// Add to backend's blkdevs array if it matches
			if (backend->dev_num < CBDB_BLKDEV_COUNT_MAX) {
				memcpy(&backend->blkdevs[backend->dev_num++], &blkdev, sizeof(struct cbd_blkdev));
			} else {
				fprintf(stderr, "Warning: Exceeded max blkdev count for backend %u\n", backend_id);
				break;
			}
		}
	}

	return 0;
}
