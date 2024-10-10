#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysfs/libsysfs.h>

#include "cbdctl.h"

#define CBDCTL_PROGRAM_NAME "cbdctl"
static void usage ()
{
    fprintf(stdout, "Usage: ");
    fprintf(stdout, "%s <sub_command> [cmd_parameters]\n\n", CBDCTL_PROGRAM_NAME);
    fprintf(stdout, "Description: \n");
    fprintf(stdout, "	cbd-utils, userspace tools used to manage CBD （CXL Block Device)\n"
		    "	Checkout the [CBD (CXL Block Device)](https://datatravelguide.github.io/dtg-blog/cbd/cbd.html) for CBD details\n\n");
    fprintf(stdout, "Sub commands:\n");
    fprintf(stdout, "\ttp_reg, transport register command\n"
		    "\t <-H|--host hostname>, assigned host name\n"
		    "\t <-d|--device device>, assigned device path\n"
		    "\t [-f|--format], format the device if specified, default false\n"
		    "\t [-F|--force], format the device if specified, default false\n"
		    "\t [-h|--help], print this message\n"
                    "\t\t\t%s tp_reg -H hostname -d device -F -f\n\n", CBDCTL_PROGRAM_NAME);
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
		cbdctl_cmd_t cmd = cbdctl_cmd_tables[i];
		if (!strncmp(cmd_str, cmd.cmd_name, strlen(cmd.cmd_name))) {
			return cmd.cmd_type;
		}
	}
	return CCT_INVALID;
}

/* cbdctl options */
static struct option long_options[] =
{
	{"help", no_argument,0, 'h'},
	{"host", required_argument,0, 'H'},
	{"device", required_argument,0, 'd'},
	{"format", no_argument, 0, 'f'},
	{"force", no_argument, 0, 'F'},
	{0, 0, 0, 0},
};

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

	if (options->co_cmd == CCT_INVALID) {
		usage();
		exit(1);
	}

	while (true) {
		int option_index = 0;

		arg = getopt_long(argc, argv, "hH:d:fF", long_options, &option_index);
		/* End of the options? */
		if (arg == -1) {
			break;
		}

		/* Find the matching case of the argument */
		switch (arg) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
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
		case 'd':
			if (!optarg || (strlen(optarg) == 0)) {
				printf("Device name is null or empty!!\n");
				usage();
				exit(EXIT_FAILURE);
			}

			strncpy(options->co_device, optarg, sizeof(options->co_device) - 1);
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

int cbdctl_transport_register(cbd_opt_t *opt)
{
	int ret = 0;
	char tr_buff[FILE_NAME_SIZE*3] = {0};
	struct sysfs_attribute *sysattr = NULL;

	if (strlen(opt->co_device) == 0 || strlen(opt->co_host) == 0) {
		printf("device or host is null!\n");
		ret = -1;
		goto err_out;
	}
	sprintf(tr_buff, "path=%s,hostname=%s,force=%d,format=%d",
		opt->co_device, opt->co_host, opt->co_force, opt->co_format);
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
