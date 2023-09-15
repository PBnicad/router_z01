#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "upgrade.h"

#define PAYLOAD_MAX_SIZE 1024
#define MSG_HEADER_SIZE  8
#define DATA_MAX_SIZE    (PAYLOAD_MAX_SIZE + MSG_HEADER_SIZE)

//volt g_uc_setparam_request  used 
enum {
	STP_GET_PARAM,
	STP_CFG_DISTANCE,
	STP_CFG_MODE,
	STP_CFG_ROLE,
	STP_CFG_CLOCK,
};
//flag
//bit 0:get param
//bit 1:set distance
//bit 2:set mode
//bit 3:set role
//bit 4:set clock sync status

/* 30秒内无数据 */
#define HEARTBEAT_PACKET_INTERVAL 600

enum {
	OP_ON_HEADER,
	OP_ON_BODY,
	OP_MAX
};

typedef struct PARAMCMD {
	char size;
	char cmdbuff[23];
}PARAM_CMD_t;


typedef struct buffer {
	uint32_t len;
	uint8_t data[DATA_MAX_SIZE];
} buffer_t;

typedef struct message {
	uint32_t magic;
	uint32_t length;
	uint8_t payload[PAYLOAD_MAX_SIZE];
} message_t;

typedef struct parser {
	uint8_t opcode;
	struct buffer buf;
	struct message msg;
} parser_t;

typedef struct sock_ctx {
	struct uart_ctx *uart;
	struct uloop_fd forwarder;
	struct uloop_timeout retry;
} sock_ctx_t;

typedef struct uart_ctx {
	struct sock_ctx *sock;
//	sock_ctx_t *test_sock;
	struct parser parser;
	struct uloop_fd forwarder;
} uart_ctx_t;


typedef enum UPGRADE_MODE_n {
	UPGRADE_START,
	UPGRADE_STOP,
}UPGRADE_MODE_n;

typedef struct {
	UPGRADE_MODE_n flag_upgrade_mode;
	sock_ctx_t *sock_ctx_t;
	uart_ctx_t *uart_ctx_t;
}ENVIRONMENT_STP_t;

extern int debug;

#endif

