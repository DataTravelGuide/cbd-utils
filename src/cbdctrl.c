#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <jansson.h>

#include "cbdctrl.h"
#include "libcbdsys.h"

#define CBDCTL_PROGRAM_NAME "cbdctrl"

static void usage ()
{
	fprintf(stdout, "usage: %s <command> [<args>]\n\n", CBDCTL_PROGRAM_NAME);
	fprintf(stdout, "Description:\n");
	fprintf(stdout, "   cbd-utils: userspace tools to manage CBD (CXL Block Device)\n");
	fprintf(stdout, "   See the documentation for details on CBD:\n");
	fprintf(stdout, "   https://datatravelguide.github.io/dtg-blog/cbd/cbd.html\n\n");

	fprintf(stdout, "These are common cbdctrl commands used in various situations:\n\n");

	fprintf(stdout, "Managing transports:\n");
	fprintf(stdout, "   tp-reg          Register a new transport\n");
	fprintf(stdout, "                   -H, --host <hostname>        Assign hostname\n");
	fprintf(stdout, "                   -p, --path <path>            Specify transport path\n");
	fprintf(stdout, "                   -f, --format                 Format path (default: false)\n");
	fprintf(stdout, "                   -F, --force                  Force format path (default: false)\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s tp-reg -H hostname -p /path -F -f\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   tp-unreg        Unregister a transport\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s tp-unreg --transport 0\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   tp-list         List all transports\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s tp-list\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "Managing hosts:\n");
	fprintf(stdout, "   host-list       List all hosts\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s host-list\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "Managing backends:\n");
	fprintf(stdout, "   backend-start   Start a backend\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -p, --path <path>            Specify backend path\n");
	fprintf(stdout, "                   -c, --cache-size <size>      Set cache size (units: K, M, G)\n");
	fprintf(stdout, "                   -n, --handlers <count>       Set handler count (max %d)\n", CBD_BACKEND_HANDLERS_MAX);
	fprintf(stdout, "                   -D, --start-dev              Start a blkdev at the same time\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s backend-start -p /path -c 512M -n 1\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   backend-stop    Stop a backend\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -b, --backend <bid>          Specify backend ID\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s backend-stop --backend 0\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   backend-list    List all backends on this host\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -a, --all                    List backends on all hosts\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s backend-list\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "Managing block devices:\n");
	fprintf(stdout, "   dev-start       Start a block device\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -b, --backend <bid>          Specify backend ID\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s dev-start --backend 0\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   dev-stop        Stop a block device\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -d, --dev <dev_id>           Specify device ID\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s dev-stop --dev 0\n\n", CBDCTL_PROGRAM_NAME);

	fprintf(stdout, "   dev-list        List all blkdevs on this host\n");
	fprintf(stdout, "                   -t, --transport <tid>        Specify transport ID\n");
	fprintf(stdout, "                   -a, --all                    List blkdevs on all hosts\n");
	fprintf(stdout, "                   -h, --help                   Print this help message\n");
	fprintf(stdout, "                   Example: %s blkdev-list\n\n", CBDCTL_PROGRAM_NAME);
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
	{"start-dev", no_argument, 0, 'D'},
	{"dev", required_argument,0, 'd'},
	{"path", required_argument,0, 'p'},
	{"format", no_argument, 0, 'f'},
	{"cache-size", required_argument,0, 'c'},
	{"handlers", required_argument,0, 'n'},
	{"force", no_argument, 0, 'F'},
	{"all", no_argument, 0, 'a'},
	{0, 0, 0, 0},
};

unsigned int opt_to_MB(const char *input)
{
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

		arg = getopt_long(argc, argv, "a:h:t:H:b:d:p:f:c:n:D:F", long_options, &option_index);
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
		case 'a':
			options->co_all = true;
			break;
		case 'D':
			options->co_start_dev = true;
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
	json_object_set_new(json_obj, "transport_id", json_integer(cbdt->transport_id));
	json_object_set_new(json_obj, "host_id", json_integer(cbdt->host_id));

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
		return -EINVAL;
	}

	sprintf(tr_buff, "path=%s,hostname=%s,force=%d,format=%d",
		opt->co_path, opt->co_host, opt->co_force, opt->co_format);

	return cbdsys_write_value(SYSFS_CBD_TRANSPORT_REGISTER, tr_buff);
}

int cbdctrl_transport_unregister(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[CBD_PATH_LEN*3] = {0};

	sprintf(tr_buff, "transport_id=%u", opt->co_transport_id);

	return cbdsys_write_value(SYSFS_CBD_TRANSPORT_UNREGISTER, tr_buff);
}

int cbdctrl_transport_list(cbd_opt_t *opt)
{
	char tr_buff[CBD_PATH_LEN * 3] = {0};
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

static int dev_start(unsigned int transport_id, unsigned int backend_id)
{
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	struct cbd_transport cbdt;
	struct cbd_backend old_backend, new_backend;
	bool found = false;
	struct cbd_blkdev *found_dev = NULL;
	int ret;

	/* Initialize transport */
	ret = cbdsys_transport_init(&cbdt, transport_id);
	if (ret)
		return ret;

	/* Get information about the current backend */
	ret = cbdsys_backend_init(&cbdt, &old_backend, backend_id);
	if (ret) {
		printf("Failed to get current backend information. Error: %d\n", ret);
		return ret;
	}

	/* Clear block devices associated with the backend */
	ret = cbdsys_backend_blkdevs_clear(&cbdt, backend_id);
	if (ret)
		return ret;

	/* Prepare the dev-start command */
	snprintf(cmd, sizeof(cmd), "op=dev-start,backend_id=%u", backend_id);

	/* Get the sysfs attribute path */
	transport_adm_path(transport_id, adm_path, sizeof(adm_path));

	ret = cbdsys_write_value(adm_path, cmd);
	if (ret)
		return ret;

	/* Get information about the backend after dev-start */
	ret = cbdsys_backend_init(&cbdt, &new_backend, backend_id);
	if (ret) {
		printf("Failed to get new backend information. Error: %d\n", ret);
		return ret;
	}

	/* Compare old_backend and new_backend to identify new block devices */
	if (new_backend.dev_num > old_backend.dev_num) {
		for (unsigned int i = 0; i < new_backend.dev_num; i++) {
			if (new_backend.blkdevs[i].host_id != cbdt.host_id)
				continue;

			for (unsigned int j = 0; j < old_backend.dev_num; j++) {
				if (new_backend.blkdevs[i].blkdev_id == old_backend.blkdevs[j].blkdev_id)
					goto next;
				break;
			}

			found_dev = &new_backend.blkdevs[i];
			break;
next:
			continue;
		}
	}

	if (!found_dev) {
		printf("No new block devices were added.\n");
		return 1;
	}

	printf("%s\n", found_dev->dev_name);
	return 0;
}

int cbdctrl_backend_start(cbd_opt_t *options) {
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	struct cbd_transport cbdt;
	unsigned int backend_id;
	int ret;

	cbdsys_transport_init(&cbdt, options->co_transport_id);

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
	ret = cbdsys_write_value(adm_path, cmd);
	if (ret)
		return ret;

	ret = cbdsys_find_backend_id_from_path(&cbdt, options->co_path, &backend_id);
	if (ret) {
		printf("Backend for host: %u path: %s not found\n", cbdt.host_id, options->co_path);
		return ret;
	}

	if (options->co_start_dev)
		return dev_start(options->co_transport_id, backend_id);

	return 0;
}

int cbdctrl_backend_stop(cbd_opt_t *options) {
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	int ret;

	if (options->co_backend_id == UINT_MAX) {
		printf("--backend required for backend-stop command\n");
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "op=backend-stop,backend_id=%u", options->co_backend_id);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));
	return cbdsys_write_value(adm_path, cmd);
}

int cbdctrl_backend_list(cbd_opt_t *options)
{
	struct cbd_transport cbdt;
	json_t *array = json_array(); // Create JSON array for backends
	if (array == NULL) {
		fprintf(stderr, "Error creating JSON array\n");
		return -1;
	}

	// Initialize cbd_transport
	int ret = cbdsys_transport_init(&cbdt, options->co_transport_id);
	if (ret < 0) {
		json_decref(array);
		return ret;
	}

	// Iterate through all backends and generate JSON object for each
	for (unsigned int i = 0; i < cbdt.backend_num; i++) {
		struct cbd_backend backend;
		ret = cbdsys_backend_init(&cbdt, &backend, i); // Initialize current backend
		if (ret < 0) {
			continue;
		}

		if (!options->co_all && backend.host_id != cbdt.host_id)
			continue;

		// Create JSON object and add fields for the backend
		json_t *json_backend = json_object();
		json_object_set_new(json_backend, "backend_id", json_integer(backend.backend_id));
		json_object_set_new(json_backend, "host_id", json_integer(backend.host_id));
		json_object_set_new(json_backend, "backend_path", json_string(backend.backend_path));
		json_object_set_new(json_backend, "alive", json_boolean(backend.alive));
		json_object_set_new(json_backend, "cache_segs", json_integer(backend.cache_segs));
		json_object_set_new(json_backend, "gc_percent", json_integer(backend.gc_percent));

		// Create JSON array for blkdevs within the backend
		json_t *json_blkdevs = json_array();
		for (unsigned int j = 0; j < backend.dev_num; j++) {
			struct cbd_blkdev *blkdev = &backend.blkdevs[j];

			// Create JSON object for each blkdev and add fields
			json_t *json_blkdev = json_object();
			json_object_set_new(json_blkdev, "blkdev_id", json_integer(blkdev->blkdev_id));
			json_object_set_new(json_blkdev, "host_id", json_integer(blkdev->host_id));
			json_object_set_new(json_blkdev, "backend_id", json_integer(blkdev->backend_id));
			json_object_set_new(json_blkdev, "dev_name", json_string(blkdev->dev_name));
			json_object_set_new(json_blkdev, "alive", json_boolean(blkdev->alive));

			// Append blkdev JSON object to the blkdevs JSON array
			json_array_append_new(json_blkdevs, json_blkdev);
		}

		// Add blkdevs array to the backend JSON object
		json_object_set_new(json_backend, "blkdevs", json_blkdevs);

		// Append backend JSON object to the main JSON array
		json_array_append_new(array, json_backend);
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

int cbdctrl_dev_start(cbd_opt_t *options) {
	if (options->co_backend_id == UINT_MAX) {
		printf("--backend required for dev-start command\n");
		return -EINVAL;
	}

	return dev_start(options->co_transport_id, options->co_backend_id);
}

#define MAX_RETRIES 3
#define RETRY_INTERVAL 500 // in milliseconds

int cbdctrl_dev_stop(cbd_opt_t *options) {
	char adm_path[CBD_PATH_LEN];
	char cmd[CBD_PATH_LEN * 3] = { 0 };
	int ret;
	int attempt;

	if (options->co_dev_id == UINT_MAX) {
		printf("--dev required for dev-stop command\n");
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "op=dev-stop,dev_id=%u", options->co_dev_id);

	transport_adm_path(options->co_transport_id, adm_path, sizeof(adm_path));

	// Retry mechanism for sysfs_write_attribute
	for (attempt = 0; attempt < MAX_RETRIES; ++attempt) {
		ret = cbdsys_write_value(adm_path, cmd);
		if (ret == 0)
			break; // Success, exit the loop

		printf("Attempt %d/%d failed to write command '%s'. Error: %s\n",
			attempt + 1, MAX_RETRIES, cmd, strerror(ret));

		// Wait before retrying
		usleep(RETRY_INTERVAL * 1000); // Convert milliseconds to microseconds
	}

	if (ret != 0) {
		printf("Failed to write command '%s' after %d attempts. Final Error: %s\n",
			cmd, MAX_RETRIES, strerror(ret));
	}

	return ret;
}

int cbdctrl_dev_list(cbd_opt_t *options)
{
	struct cbd_transport cbdt;
	json_t *array = json_array(); // Create JSON array
	if (array == NULL) {
		fprintf(stderr, "Error creating JSON array\n");
		return -1;
	}

	// Initialize cbd_transport
	int ret = cbdsys_transport_init(&cbdt, options->co_transport_id);
	if (ret < 0) {
		json_decref(array);
		return ret;
	}

	// Iterate through all blkdevs and generate JSON object for each
	for (unsigned int i = 0; i < cbdt.blkdev_num; i++) {
		struct cbd_blkdev blkdev;
		ret = cbdsys_blkdev_init(&cbdt, &blkdev, i); // Initialize current blkdev
		if (ret < 0) {
			continue;
		}

		if (!options->co_all && blkdev.host_id != cbdt.host_id)
			continue;

		// Create JSON object and add fields
		json_t *json_blkdev = json_object();
		json_object_set_new(json_blkdev, "blkdev_id", json_integer(blkdev.blkdev_id));
		json_object_set_new(json_blkdev, "host_id", json_integer(blkdev.host_id));
		json_object_set_new(json_blkdev, "backend_id", json_integer(blkdev.backend_id));
		json_object_set_new(json_blkdev, "dev_name", json_string(blkdev.dev_name));
		json_object_set_new(json_blkdev, "alive", json_boolean(blkdev.alive));

		// Append JSON object to JSON array
		json_array_append_new(array, json_blkdev);
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
