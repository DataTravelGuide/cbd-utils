#ifndef CBDCTL_H
#define CBDCTL_H


#include <stdbool.h>
#include <getopt.h>

/* Max size of a file name */
#define FILE_NAME_SIZE 256

#define CBDCTL_TRANSPORT_REGISTER "tp_reg"
#define CBDCTL_BACKEND_START "backend-start"
#define CBDCTL_BACKEND_STOP "backend-stop"

#define SYSFS_CBD_TRANSPORT_REGISTER "/sys/bus/cbd/transport_register"
#define SYSFS_TRANSPORT_BASE_PATH "/sys/bus/cbd/devices/transport"

#define CBD_BACKEND_HANDLERS_MAX 128

enum CBDCTL_CMD_TYPE {
	CCT_TRANSPORT_REGISTER	= 0,
	CCT_BACKEND_START,
	CCT_BACKEND_STOP,
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
	unsigned int		co_transport_id;
	unsigned int		co_backend_id;
};

/* Exports options as a global type */
typedef struct cbd_options cbd_opt_t;

typedef struct {
	const char           *cmd_name;
	enum CBDCTL_CMD_TYPE  cmd_type;
} cbdctrl_cmd_t;

static cbdctrl_cmd_t cbdctrl_cmd_tables[] = {
	{CBDCTL_TRANSPORT_REGISTER, CCT_TRANSPORT_REGISTER},
	{CBDCTL_BACKEND_START, CCT_BACKEND_START},
	{CBDCTL_BACKEND_STOP, CCT_BACKEND_STOP},
	{"", CCT_INVALID},
};


/* Public functions section */

enum CBDCTL_CMD_TYPE cbd_get_cmd_type(char *cmd_str);

void cbd_options_parser(int argc, char* argv[], cbd_opt_t* options);

int cbdctrl_transport_register(cbd_opt_t *options);
int cbdctrl_backend_start(cbd_opt_t *options);
int cbdctrl_backend_stop(cbd_opt_t *options);

static inline void transport_adm_path(int transport_id, char *buffer, size_t buffer_size)
{
	/* Generate the path with transport_id */
	snprintf(buffer, buffer_size, "%s%d/adm", SYSFS_TRANSPORT_BASE_PATH, transport_id);
}

#endif // CBDCTL_H
