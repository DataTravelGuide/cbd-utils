#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sysfs/libsysfs.h>

#include "cbdctrl.h"

#define CBDCTL_PROGRAM_NAME "cbdctrl"
static void usage ()
{
    fprintf(stdout, "Usage: ");
    fprintf(stdout, "%s <sub_command> [cmd_parameters]\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "Description: \n");
    fprintf(stdout, "	cbd-utils, userspace tools used to manage CBD （CXL Block Device)\n"
		    "	Checkout the [CBD (CXL Block Device)](https://datatravelguide.github.io/dtg-blog/cbd/cbd.html) for CBD details\n\n");
    fprintf(stdout, "Sub commands:\n");
    fprintf(stdout, "\ttp-reg, transport register command\n"
		    "\t <-H|--host hostname>, assigned host name\n"
		    "\t <-p|--path path>, assigned path for transport\n"
		    "\t [-f|--format], format the path if specified, default false\n"
		    "\t [-F|--force], format the path if specified, default false\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s tp_reg -H hostname -p path -F -f\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\ttp-unreg, transport unregister command\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s tp_reg -H hostname -p path -F -f\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\tbackend-start, start a backend\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t <-p|--path path>, assigned path for backend\n"
		    "\t <-c|--cache-size size>, cache size, units (K|M|G)\n"
		    "\t <-n|--handlers count>, handler count (max %d)\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s backend-start -p path -c 512M -n 1\n\n", CBD_BACKEND_HANDLERS_MAX, CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\tbackend-stop, stop a backend\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t <-b|--backend bid>, backend id\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s backend-stop --backend 0\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\tdev-start, start a block device\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t <-b|--backend bid>, backend id\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s dev-start --backend 0\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\tdev-stop, stop a block device\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t <-d|--dev dev_id>, dev id\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s dev-stop --dev 0\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\tgc, clean up all dead objects (hosts, backends, blkdevs)\n"
		    "\t <-t|--transport tid>, transport id\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s gc -t tid\n\n", CBDCTL_PROGRAM_NAME);
}

static void cbd_options_init(cbd_opt_t* options)
{
	memset(options, 0, sizeof(cbd_opt_t));
}

enum CBDCTL_CMD_TYPE cbd_get_cmd_type(char *cmd_str)
{
	int i;

	if (!cmd_str || (strlen(cmd_str) == 0)) {
		return CCT_INVALID;
	}

	for (i = 0; i <= CCT_INVALID; i++) {
		cbdctrl_cmd_t cmd = cbdctrl_cmd_tables[i];
		if (!strncmp(cmd_str, cmd.cmd_name, strlen(cmd.cmd_name))) {
			return cmd.cmd_type;
		}
	}
	return CCT_INVALID;
}

/* cbdctrl options */
static struct option long_options[] =
{
	{"help", no_argument,0, 'h'},
	{"transport", required_argument,0, 't'},
	{"host", required_argument,0, 'H'},
	{"backend", required_argument,0, 'b'},
	{"dev", required_argument,0, 'd'},
	{"path", required_argument,0, 'p'},
	{"format", no_argument, 0, 'f'},
	{"cache-size", required_argument,0, 'c'},
	{"handlers", required_argument,0, 'n'},
	{"force", no_argument, 0, 'F'},
	{0, 0, 0, 0},
};

unsigned int opt_to_MB(const char *input) {
	char *endptr;
	unsigned long size = strtoul(input, &endptr, 10);

	/* Convert to MiB based on unit suffix */
	if (*endptr != '\0') {
		if (strcasecmp(endptr, "K") == 0 || strcasecmp(endptr, "KiB") == 0) {
			size = (size + 1023) / 1024;  /* Convert KiB to MiB, rounding up */
		} else if (strcasecmp(endptr, "M") == 0 || strcasecmp(endptr, "MiB") == 0) {
			/* Already in MiB, no conversion needed */
		} else if (strcasecmp(endptr, "G") == 0 || strcasecmp(endptr, "GiB") == 0) {
			size *= 1024;  /* Convert GiB to MiB */
		} else {
			fprintf(stderr, "Invalid unit for cache size: %s\n", endptr);
			exit(EXIT_FAILURE);
		}
	} else {
		/* Assume bytes if no unit; convert to MiB, rounding up */
		size = (size + (1024 * 1024 - 1)) / (1024 * 1024);
	}

	return (unsigned int)size;
}

/*
 * Public function that loops until command line options were parsed
 */
void cbd_options_parser(int argc, char* argv[], cbd_opt_t* options)
{
	int arg; /* Current option */

	if (argc < 2) {
		usage();
		exit(1);
	}
	cbd_options_init(options);
	options->co_cmd = cbd_get_cmd_type(argv[1]);
	options->co_backend_id = UINT_MAX;
	options->co_dev_id = UINT_MAX;

	if (options->co_cmd == CCT_INVALID) {
		usage();
		exit(1);
	}

	while (true) {
		int option_index = 0;

		arg = getopt_long(argc, argv, "h:t:H:b:d:p:f:c:n:F", long_options, &option_index);
		/* End of the options? */
		if (arg == -1) {
			break;
		}

		/* Find the matching case of the argument */
		switch (arg) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 't':
			options->co_transport_id = strtoul(optarg, NULL, 10);
			break;
		case 'f':
			options->co_format = true;
			break;
		case 'F':
			options->co_force = true;
			break;
		case 'H':
			if (!optarg || (strlen(optarg) == 0)) {
				printf("Host name is null or empty!!\n");
				usage();
				exit(EXIT_FAILURE);
			}

			strncpy(options->co_host, optarg, sizeof(options->co_host) - 1);
			break;
		case 'b':
			options->co_backend_id = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			options->co_dev_id = strtoul(optarg, NULL, 10);
			break;
		case 'p':
			if (!optarg || (strlen(optarg) == 0)) {
				printf("path is null or empty!!\n");
				usage();
				exit(EXIT_FAILURE);
			}

			strncpy(options->co_path, optarg, sizeof(options->co_path) - 1);
			break;
		case 'c':
			options->co_cache_size = opt_to_MB(optarg);
			break;

		case 'n':
			options->co_handlers = strtoul(optarg, NULL, 10);
			if (options->co_handlers > CBD_BACKEND_HANDLERS_MAX) {
				printf("Handlers exceed maximum of %d!\n", CBD_BACKEND_HANDLERS_MAX);
				usage();
				exit(EXIT_FAILURE);
			}
			break;
		case '?':
			usage();
			exit(EXIT_FAILURE);
		default:
			usage();
			exit(1);
		}
	}
}

int cbdctrl_transport_register(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[FILE_NAME_SIZE*3] = {0};
	struct sysfs_attribute *sysattr = NULL;

	if (strlen(opt->co_path) == 0 || strlen(opt->co_host) == 0) {
		printf("path or host is null!\n");
		ret = -1;
		goto err_out;
	}
	sprintf(tr_buff, "path=%s,hostname=%s,force=%d,format=%d",
		opt->co_path, opt->co_host, opt->co_force, opt->co_format);
	sysattr = sysfs_open_attribute(SYSFS_CBD_TRANSPORT_REGISTER);
	if (sysattr == NULL) {
		printf("failed to open %s, exit!\n", SYSFS_CBD_TRANSPORT_REGISTER);
		ret = -1;
		goto err_out;
	}

	ret = sysfs_write_attribute(sysattr, tr_buff, sizeof(tr_buff));
	if (ret != 0) {
		printf("failed to write %s to %s, exit!\n", tr_buff, SYSFS_CBD_TRANSPORT_REGISTER);
		ret = -1;
	}
err_out:
	if (sysattr != NULL) {
		sysfs_close_attribute(sysattr);
	}
	return ret;
}

int cbdctrl_transport_unregister(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[FILE_NAME_SIZE*3] = {0};
	struct sysfs_attribute *sysattr = NULL;

	sprintf(tr_buff, "transport_id=%u", opt->co_transport_id);
	sysattr = sysfs_open_attribute(SYSFS_CBD_TRANSPORT_UNREGISTER);
	if (sysattr == NULL) {
		printf("failed to open %s, exit!\n", SYSFS_CBD_TRANSPORT_UNREGISTER);
		ret = -1;
		goto err_out;
	}

	ret = sysfs_write_attribute(sysattr, tr_buff, sizeof(tr_buff));
	if (ret != 0) {
		printf("failed to write %s to %s, exit!\n", tr_buff, SYSFS_CBD_TRANSPORT_UNREGISTER);
		ret = -1;
	}
err_out:
	if (sysattr != NULL) {
		sysfs_close_attribute(sysattr);
	}
	return ret;
}

int cbdctrl_backend_start(cbd_opt_t *options) {
	char adm_path[FILE_NAME_SIZE];
	char cmd[FILE_NAME_SIZE * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;

	snprintf(cmd, sizeof(cmd), "op=backend-start,path=%s", options->co_path);

	if (options->co_cache_size != 0)
	    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), ",cache_size=%u", options->co_cache_size);

	if (options->co_handlers != 0)
	    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), ",handlers=%u", options->co_handlers);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));
	sysattr = sysfs_open_attribute(adm_path);
	if (!sysattr) {
		printf("Failed to open '%s'\n", adm_path);
		return -1;
	}

	ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));
	sysfs_close_attribute(sysattr);
	if (ret != 0) {
		printf("Failed to write command '%s'. Error: %s\n", cmd, strerror(ret));
	}

	return ret;
}

int cbdctrl_backend_stop(cbd_opt_t *options) {
	char adm_path[FILE_NAME_SIZE];
	char cmd[FILE_NAME_SIZE * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;

	if (options->co_backend_id == UINT_MAX) {
		printf("--backend required for backend-stop command\n");
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "op=backend-stop,backend_id=%u", options->co_backend_id);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));
	sysattr = sysfs_open_attribute(adm_path);
	if (!sysattr) {
		printf("Failed to open '%s'\n", adm_path);
		return -1;
	}

	ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));
	sysfs_close_attribute(sysattr);
	if (ret != 0) {
		printf("Failed to write command '%s'. Error: %s\n", cmd, strerror(ret));
	}

	return ret;
}

int cbdctrl_dev_start(cbd_opt_t *options) {
	char adm_path[FILE_NAME_SIZE];
	char cmd[FILE_NAME_SIZE * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;

	if (options->co_backend_id == UINT_MAX) {
		printf("--backend required for dev-start command\n");
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "op=dev-start,backend_id=%u", options->co_backend_id);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));
	sysattr = sysfs_open_attribute(adm_path);
	if (!sysattr) {
		printf("Failed to open '%s'\n", adm_path);
		return -1;
	}

	ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));
	sysfs_close_attribute(sysattr);
	if (ret != 0) {
		printf("Failed to write command '%s'. Error: %s\n", cmd, strerror(ret));
	}

	return ret;
}

#define MAX_RETRIES 3
#define RETRY_INTERVAL 500 // in milliseconds

int cbdctrl_dev_stop(cbd_opt_t *options) {
	char adm_path[FILE_NAME_SIZE];
	char cmd[FILE_NAME_SIZE * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;
	int attempt;

	if (options->co_dev_id == UINT_MAX) {
		printf("--dev required for dev-stop command\n");
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "op=dev-stop,dev_id=%u", options->co_dev_id);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));
	sysattr = sysfs_open_attribute(adm_path);
	if (!sysattr) {
		printf("Failed to open '%s'\n", adm_path);
		return -1;
	}

	// Retry mechanism for sysfs_write_attribute
	for (attempt = 0; attempt < MAX_RETRIES; ++attempt) {
		ret = sysfs_write_attribute(sysattr, cmd, strlen(cmd));
		if (ret == 0) {
			break; // Success, exit the loop
		}

		printf("Attempt %d/%d failed to write command '%s'. Error: %s\n",
			attempt + 1, MAX_RETRIES, cmd, strerror(ret));

		// Wait before retrying
		usleep(RETRY_INTERVAL * 1000); // Convert milliseconds to microseconds
	}

	sysfs_close_attribute(sysattr);

	if (ret != 0) {
		printf("Failed to write command '%s' after %d attempts. Final Error: %s\n",
			cmd, MAX_RETRIES, strerror(ret));
	}

	return ret;
}

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

int cbdctrl_gc(cbd_opt_t *options) {
	int ret;

	ret = blkdevs_clean(options->co_transport_id);
	if (ret)
		goto out;

	ret = backends_clean(options->co_transport_id);
	if (ret)
		goto out;

	ret = hosts_clean(options->co_transport_id);
	if (ret)
		goto out;
out:
	return ret;
}
