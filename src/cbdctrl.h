#ifndef CBDCTRL_H
#define CBDCTRL_H

#include <stdbool.h>
#include <getopt.h>


/* Max size of a file name */
#define CBD_PATH_LEN 256
#define CBD_TRANSPORT_MAX       1024                        /* Maximum number of transport instances */

#define CBDCTL_TRANSPORT_REGISTER "tp-reg"
#define CBDCTL_TRANSPORT_UNREGISTER "tp-unreg"
#define CBDCTL_TRANSPORT_LIST "tp-list"
#define CBDCTL_HOST_LIST "host-list"
#define CBDCTL_BACKEND_START "backend-start"
#define CBDCTL_BACKEND_STOP "backend-stop"
#define CBDCTL_DEV_START "dev-start"
#define CBDCTL_DEV_STOP "dev-stop"
#define CBDCTL_DEV_LIST "dev-list"

#define CBD_BACKEND_HANDLERS_MAX 128

enum CBDCTL_CMD_TYPE {
	CCT_TRANSPORT_REGISTER	= 0,
	CCT_TRANSPORT_UNREGISTER,
	CCT_TRANSPORT_LIST,
	CCT_HOST_LIST,
	CCT_BACKEND_START,
	CCT_BACKEND_STOP,
	CCT_DEV_START,
	CCT_DEV_STOP,
	CCT_DEV_LIST,
	CCT_INVALID,
};

/* Defines the cbd command line allowed options struct */
struct cbd_options
{
	enum CBDCTL_CMD_TYPE	co_cmd;
	char 			co_host[CBD_PATH_LEN];
	char			co_path[CBD_PATH_LEN];
	bool			co_force;
	bool			co_format;
	unsigned int		co_cache_size;
	unsigned int		co_handlers;
	unsigned int		co_transport_id;
	unsigned int		co_backend_id;
	unsigned int		co_dev_id;
};

/* Exports options as a global type */
typedef struct cbd_options cbd_opt_t;

typedef struct {
	const char           *cmd_name;
	enum CBDCTL_CMD_TYPE  cmd_type;
} cbdctrl_cmd_t;

static cbdctrl_cmd_t cbdctrl_cmd_tables[] = {
	{CBDCTL_TRANSPORT_REGISTER, CCT_TRANSPORT_REGISTER},
	{CBDCTL_TRANSPORT_UNREGISTER, CCT_TRANSPORT_UNREGISTER},
	{CBDCTL_TRANSPORT_LIST, CCT_TRANSPORT_LIST},
	{CBDCTL_HOST_LIST, CCT_HOST_LIST},
	{CBDCTL_BACKEND_START, CCT_BACKEND_START},
	{CBDCTL_BACKEND_STOP, CCT_BACKEND_STOP},
	{CBDCTL_DEV_START, CCT_DEV_START},
	{CBDCTL_DEV_STOP, CCT_DEV_STOP},
	{CBDCTL_DEV_LIST, CCT_DEV_LIST},
	{"", CCT_INVALID},
};


/* Public functions section */

enum CBDCTL_CMD_TYPE cbd_get_cmd_type(char *cmd_str);

void cbd_options_parser(int argc, char* argv[], cbd_opt_t* options);

int cbdctrl_transport_register(cbd_opt_t *options);
int cbdctrl_transport_unregister(cbd_opt_t *opt);
int cbdctrl_transport_list(cbd_opt_t *opt);
int cbdctrl_host_list(cbd_opt_t *opt);
int cbdctrl_backend_start(cbd_opt_t *options);
int cbdctrl_backend_stop(cbd_opt_t *options);
int cbdctrl_dev_start(cbd_opt_t *options);
int cbdctrl_dev_stop(cbd_opt_t *options);
int cbdctrl_dev_list(cbd_opt_t *options);

#endif // CBDCTRL_H
