#include <stdio.h>
#include <stdlib.h>

#include "cbdctrl.h"

static int cbdctrl_run(cbd_opt_t *options) {
	int ret = 0;
	switch (options->co_cmd) {
		case CCT_TRANSPORT_REGISTER:
			ret = cbdctrl_transport_register(options);
			break;
		case CCT_TRANSPORT_UNREGISTER:
			ret = cbdctrl_transport_unregister(options);
			break;
		case CCT_BACKEND_START:
			ret = cbdctrl_backend_start(options);
			break;
		case CCT_BACKEND_STOP:
			ret = cbdctrl_backend_stop(options);
			break;
		case CCT_DEV_START:
			ret = cbdctrl_dev_start(options);
			break;
		case CCT_DEV_STOP:
			ret = cbdctrl_dev_stop(options);
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

