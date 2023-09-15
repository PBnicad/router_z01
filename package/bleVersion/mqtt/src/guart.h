#ifndef _GUART_H
#define _GUART_H

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>


#include <assert.h>

#include <pthread.h>
#include <mosquitto.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

//#include <libubox/ulog.h>
#include <libubox/usock.h>
#include <libubox/uloop.h>


#include <json-c/json.h>
#include <libubox/md5.h>
#include <libubox/list.h>
#include <libubox/utils.h>

#include <json-c/json.h>

#include "serial.h"
#include "gjson.h"
#include "utils.h"
#include "packet.h"

#include "com.h"


#include "shell.h"
#include "guci.h"


//libubox/ulog and syslog can only use one, here use syslog
//#define ULOG_INFO	//LOGI
//#define	ULOG_ERR	//LOGE

typedef struct tty_ctx {

	struct uloop_fd fd;
	struct uloop_timeout timer;
	struct uloop_timeout MonitoringWifi;
	
	struct cositea_packet pkt;

	struct mosquitto *mosq;

	buffer_t *buf;
} tty_ctx_t;

extern void cosCreateThread(struct mosquitto *mosq);
extern void cloud_state_report(int fd, bool state);


#endif




