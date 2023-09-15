#ifndef COSITEA_H
#define COSITEA_H

#include <libubox/usock.h>
#include <libubox/uloop.h>

#include "packet.h"


#define KEEPALIVE_MSEC             2000
#define TTY_TIMER_MSEC             1000 /* !!!Cannot less than 1000 */
#define WIFI_CHECK_MSEC            20000
#define WIFI_GPRS_MSEC			   1800000


typedef struct tty_ctx {
	struct sock_ctx *sock;

	struct uloop_fd fd;
	struct uloop_timeout timer;

	struct cositea_packet pkt;

	buffer_t *buf;
} tty_ctx_t;

typedef struct sock_ctx {
	struct tty_ctx *tty;

	struct uloop_fd fd;
	struct uloop_timeout keepalive;
	struct uloop_timeout MonitoringWifi;

	buffer_t *buf;

	char *host;
	char *service;
	int connected;
} sock_ctx_t;

extern void sock_disconnected(sock_ctx_t *sock_ctx);


#endif
