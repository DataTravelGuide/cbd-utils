#include <stdio.h>
#include <stdlib.h>

#include "cbdctl.h"

static int cbdctl_run(cbd_opt_t *options)
{
	int ret = 0;

	switch(options->co_cmd) {
		case CCT_TRANSPORT_REGISTER: {
			ret = cbdctl_transport_register(options);
			break;
		};
		default:
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

	ret = cbdctl_run(&options);

	return ret;
}

