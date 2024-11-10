#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sysfs/libsysfs.h>
#include <jansson.h> 

#include "cbdctrl.h"
#include "libcbdsys.h"

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
                    "\t\t\t%s tp_unreg --transport 0\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\ttp-list, transport list command\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s tp-list\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "\thost-list, host list command\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s host-list\n\n", CBDCTL_PROGRAM_NAME);
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
	options->co_handlers = UINT_MAX;
	options->co_transport_id = 0;

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

void trim_newline(char *str) {
	size_t len = strlen(str);
	if (len > 0 && str[len - 1] == '\n') {
		str[len - 1] = '\0';
	}
}

json_t *cbd_transport_to_json(struct cbd_transport *cbdt) {
	/* Create a new JSON object */
	json_t *json_obj = json_object();

	/* Remove trailing newline from path */
	trim_newline(cbdt->path);

	/* Format magic as a hexadecimal string */
	char magic_str[19]; // 16 digits + "0x" prefix + null terminator
	snprintf(magic_str, sizeof(magic_str), "0x%016lx", cbdt->magic);

	char flags_str[11]; // 8 digits + "0x" prefix + null terminator
	snprintf(flags_str, sizeof(flags_str), "0x%08x", cbdt->flags);

	/* Add each field to the JSON object */
	json_object_set_new(json_obj, "magic", json_string(magic_str));

	json_object_set_new(json_obj, "version", json_integer(cbdt->version));
	json_object_set_new(json_obj, "flags", json_string(flags_str));
	json_object_set_new(json_obj, "host_area_off", json_integer(cbdt->host_area_off));
	json_object_set_new(json_obj, "bytes_per_host_info", json_integer(cbdt->bytes_per_host_info));
	json_object_set_new(json_obj, "host_num", json_integer(cbdt->host_num));
	json_object_set_new(json_obj, "backend_area_off", json_integer(cbdt->backend_area_off));
	json_object_set_new(json_obj, "bytes_per_backend_info", json_integer(cbdt->bytes_per_backend_info));
	json_object_set_new(json_obj, "backend_num", json_integer(cbdt->backend_num));
	json_object_set_new(json_obj, "blkdev_area_off", json_integer(cbdt->blkdev_area_off));
	json_object_set_new(json_obj, "bytes_per_blkdev_info", json_integer(cbdt->bytes_per_blkdev_info));
	json_object_set_new(json_obj, "blkdev_num", json_integer(cbdt->blkdev_num));
	json_object_set_new(json_obj, "segment_area_off", json_integer(cbdt->segment_area_off));
	json_object_set_new(json_obj, "bytes_per_segment", json_integer(cbdt->bytes_per_segment));
	json_object_set_new(json_obj, "segment_num", json_integer(cbdt->segment_num));

	/* Add path as a JSON string */
	json_object_set_new(json_obj, "path", json_string(cbdt->path));

	return json_obj;
}

int cbdctrl_transport_register(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[CBD_PATH_LEN*3] = {0};
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

	struct cbd_transport cbdt = { 0 };

	ret = cbdsys_transport_init(&cbdt, 0);
	if (ret)
		printf("ret of cbdsys_transport_init(): %d\n", ret);

	cbd_transport_to_json(&cbdt);

	return ret;
}

int cbdctrl_transport_unregister(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[CBD_PATH_LEN*3] = {0};
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

int cbdctrl_transport_list(cbd_opt_t *opt)
{
	char tr_buff[CBD_PATH_LEN * 3] = {0};
	struct sysfs_attribute *sysattr = NULL;
	struct cbd_transport cbdt;
	json_t *array = json_array();
	int ret = 0;

	for (int i = 0; i < CBD_TRANSPORT_MAX; i++) {
		ret = cbdsys_transport_init(&cbdt, i);
		if (ret == -ENOENT) {
			ret = 0;
			break;
		}

		if (ret < 0) {
			json_decref(array);
			return ret;
		}

		// Convert transport to JSON and add to array
		json_t *json_obj = cbd_transport_to_json(&cbdt);
		json_array_append_new(array, json_obj);
	}
	// Print the JSON array
	char *json_str = json_dumps(array, JSON_INDENT(4));
	printf("%s\n", json_str);

	// Clean up
	json_decref(array);
	free(json_str);

	return ret;
}

int cbdctrl_host_list(cbd_opt_t *opt)
{
	struct cbd_transport cbdt;
	json_t *array = json_array(); // Create JSON array
	if (array == NULL) {
		fprintf(stderr, "Error creating JSON array\n");
		return -1;
	}

	// Initialize cbd_transport
	int ret = cbdsys_transport_init(&cbdt, opt->co_transport_id);
	if (ret < 0) {
		json_decref(array);
		return ret;
	}

	// Iterate through all hosts and generate JSON object for each
	for (unsigned int i = 0; i < cbdt.host_num; i++) {
		struct cbd_host host;
		ret = cbdsys_host_init(&cbdt, &host, i); // Initialize current host
		if (ret < 0) {
			continue;
		}

		// Create JSON object and add fields
		json_t *json_host = json_object();
		json_object_set_new(json_host, "host_id", json_integer(host.host_id));
		json_object_set_new(json_host, "hostname", json_string(host.hostname));
		json_object_set_new(json_host, "alive", json_boolean(host.alive));

		// Append JSON object to JSON array
		json_array_append_new(array, json_host);
	}

	// Convert JSON array to a formatted string and print to stdout
	char *json_str = json_dumps(array, JSON_INDENT(4));
	if (json_str != NULL) {
		printf("%s\n", json_str);
		free(json_str);
	}

	json_decref(array); // Free JSON array memory
	return 0;
}

int cbdctrl_backend_start(cbd_opt_t *options) {
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;

	snprintf(cmd, sizeof(cmd), "op=backend-start,path=%s", options->co_path);

	if (options->co_cache_size != 0)
	    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), ",cache_size=%u", options->co_cache_size);

	if (options->co_handlers != UINT_MAX)
	    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), ",handlers=%u", options->co_handlers);

	if (options->co_backend_id != UINT_MAX) {
		printf("backend-start dont accept --backend option.\n");
		return -EINVAL;
	}

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
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
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
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	struct sysfs_attribute *sysattr;
	int ret;

	if (options->co_backend_id == UINT_MAX) {
		printf("--backend required for dev-start command\n");
		return -EINVAL;
	}

	backend_blkdevs_clear(options->co_transport_id, options->co_backend_id);

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
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
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
