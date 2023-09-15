#include "config_init.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

enum {
	GETOPT_VAL_HELP,
};

void printUsage(void)
{
	printf("gl_mqtt is a simple mqtt client that will deal with publish and subscribe message in the background.\n");
	printf("Usage: gl_mqtt {[-h host] [-p port] [-u username [-P password]]}\n");
	printf("                     [-k keepalive]\n");
	printf("       gl_mqtt --help\n\n");
	printf(" -d : enable debug messages.\n");
	printf(" -f : run in the background.\n");
	printf(" -h : mqtt host to connect to. Defaults to localhost.\n");
	printf(" -k : keep alive in seconds for this client. Defaults to 60.\n");
	printf(" -p : network port to connect to. Defaults to 1883 for plain MQTT and 8883 for MQTT over TLS.\n");
	printf(" -u : provide a username\n");
	printf(" -P : provide a password\n");
	printf(" -s : mqtt sub\n");
	printf(" -l : mqtt pub\n");
	printf(" -i : mqtt clientId\n");
	printf(" --help : display this message.\n");
}

void configInit(ACCESS_CONTRL_CONFIG_t *cfg, int argc, char **argv)
{
	int c;
	int rc;
	int opterr = 0;
	
	memset(cfg, 0, sizeof(*cfg));

	//cfg->id = get_mac(false);
	/* cfg->clean_session = true; */
	cfg->port = 1883;
	cfg->host = strdup("localhost");
	cfg->keepalive = 60;
	cfg->mode = true;

	static struct option long_options[] = {
		{ "port", 		required_argument, 	NULL, 'p' },
		{ "host", 		required_argument, 	NULL, 'h' },
		{ "keepalive",	required_argument, 	NULL, 'k' },
		{ "debug",		no_argument, 		NULL, 'd' },
		{ "disable-clean-session", no_argument, NULL, 'c' },
		{ "username",	required_argument, 	NULL, 'u' },
		{ "password", 	required_argument, 	NULL, 'P' },
		{ "sub", 		required_argument, 	NULL, 's' },
		{ "pub", 		required_argument, 	NULL, 'l' },
		{ "id", 		required_argument, 	NULL, 'i' },
		{ "mode", 		required_argument, 	NULL, 'm' },
		{ "version", 	required_argument, 	NULL, 'v' },
		{ "pid", 		required_argument, 	NULL, 'f' },
		{ "help", 		no_argument, 		NULL, GETOPT_VAL_HELP },
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "p:h:k:d:u:P:s:l:i:m:v:f", long_options, NULL)) != -1) {
		switch (c) {
		case GETOPT_VAL_HELP:
			printUsage();
			exit(EXIT_SUCCESS);
			break;
		case 'p':
			cfg->port = atoi(optarg);
			break;
		case 'h':
			cfg->host = strdup(optarg);
			break;
		case 'k':
			cfg->keepalive = atoi(optarg);
			break;
		case 'd':
			cfg->debug = atoi(optarg);
			break;
		case 'c':
			/* cfg->clean_session = false; */
			break;
		case 'u':
			cfg->username = strdup(optarg);
			break;
		case 'P':
			cfg->password = strdup(optarg);
			break;
		case 's':
			cfg->sub = strdup(optarg);
			break;
		case 'l':
			cfg->pub = strdup(optarg);
			break;
		case 'm':
			cfg->mode = atoi(optarg);
			break;
		case 'i':
			cfg->id = strdup(optarg);
			break;
		case 'v':
			cfg->version = atoi(optarg);
			break;
		case 'f':
			cfg->pid_path = strdup(optarg);
			break;
		case '?':
			opterr = 1;
			break;
		}
	}

	if (opterr) {
		printUsage();
		exit(EXIT_FAILURE);
	}
}