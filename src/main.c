#include <stdio.h>
#include <stdlib.h>

#include "cbdctrl.h"

/* Function to check if a kernel module is loaded */
static int is_module_loaded(const char *module_name)
{
	char command[128];
	snprintf(command, sizeof(command), "lsmod | grep -q '^%s '", module_name);
	return system(command) == 0;
}

/* Function to load a kernel module */
static int load_module(const char *module_name)
{
	char command[128];
	snprintf(command, sizeof(command), "modprobe %s", module_name);
	return system(command);
}

static int cbdctrl_run(cbd_opt_t *options)
{
	int ret = 0;

	/* Check if 'cbd' module is loaded */
	if (!is_module_loaded("cbd")) {
		if (load_module("cbd") != 0) {
			fprintf(stderr, "Failed to load 'cbd' module. Exiting.\n");
			return -1; /* Return an error if module cannot be loaded */
		}
	}

	switch (options->co_cmd) {
		case CCT_TRANSPORT_REGISTER:
			ret = cbdctrl_transport_register(options);
			break;
		case CCT_TRANSPORT_UNREGISTER:
			ret = cbdctrl_transport_unregister(options);
			break;
		case CCT_TRANSPORT_LIST:
			ret = cbdctrl_transport_list(options);
			break;
		case CCT_HOST_LIST:
			ret = cbdctrl_host_list(options);
			break;
		case CCT_BACKEND_START:
			ret = cbdctrl_backend_start(options);
			break;
		case CCT_BACKEND_STOP:
			ret = cbdctrl_backend_stop(options);
			break;
		case CCT_BACKEND_LIST:
			ret = cbdctrl_backend_list(options);
			break;
		case CCT_DEV_START:
			ret = cbdctrl_dev_start(options);
			break;
		case CCT_DEV_STOP:
			ret = cbdctrl_dev_stop(options);
			break;
		case CCT_DEV_LIST:
			ret = cbdctrl_dev_list(options);
			break;
		default:
			printf("Unknown command: %u\n", options->co_cmd);
			ret = -1;
			break;
	}
	return ret;
}

int main (int argc, char* argv[])
{
	int ret;
	cbd_opt_t options;

	cbd_options_parser(argc, argv, &options);

	ret = cbdctrl_run(&options);

	return ret;
}

