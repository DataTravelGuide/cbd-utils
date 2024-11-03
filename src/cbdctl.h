#ifndef CBDCTL_H
#define CBDCTL_H


#include <stdbool.h>
#include <getopt.h>

/* Max size of a file name */
#define FILE_NAME_SIZE 256

#define CBDCTL_TRANSPORT_REGISTER "tp_reg"

#define SYSFS_CBD_TRANSPORT_REGISTER "/sys/bus/cbd/transport_register"

#define CBDCTL_BACKEND_START "backend-start"
#define CBD_BACKEND_HANDLERS_MAX 128

enum CBDCTL_CMD_TYPE {
	CCT_TRANSPORT_REGISTER	= 0,
	CCT_BACKEND_START,
	CCT_INVALID,
};

/* Defines the cbd command line allowed options struct */
struct cbd_options
{
	enum CBDCTL_CMD_TYPE	co_cmd;
	char 			co_host[FILE_NAME_SIZE];
	char			co_device[FILE_NAME_SIZE];
	bool			co_force;
	bool			co_format;
	unsigned int		co_cache_size;
	unsigned int		co_handlers;
};

/* Exports options as a global type */
typedef struct cbd_options cbd_opt_t;

typedef struct {
	const char           *cmd_name;
	enum CBDCTL_CMD_TYPE  cmd_type;
} cbdctl_cmd_t;

static cbdctl_cmd_t cbdctl_cmd_tables[] = {
	{CBDCTL_TRANSPORT_REGISTER, CCT_TRANSPORT_REGISTER},
	{CBDCTL_BACKEND_START, CCT_BACKEND_START},
	{"", CCT_INVALID},
};


/* Public functions section */

enum CBDCTL_CMD_TYPE cbd_get_cmd_type(char *cmd_str);

void cbd_options_parser(int argc, char* argv[], cbd_opt_t* options);

int cbdctl_transport_register(cbd_opt_t *options);
int cbdctl_backend_start(cbd_opt_t *options);

#endif // CBDCTL_H