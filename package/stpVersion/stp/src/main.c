#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>


#include <libubox/ulog.h>
#include <libubox/uloop.h>
#include <libubox/usock.h>

//#include <curl/curl.h>

#include <pthread.h>
#include <uci.h>

#include "serial.h"
#include "shell.h"
#include "upgrade.h"
#include "config.h"
#include "proofreadMac.h"

#define TEST_SOCK_CODE 0

ENVIRONMENT_STP_t g_Stp_env_t = {
	.flag_upgrade_mode = UPGRADE_STOP,
};

static int port         = 0;
static int use_udp      = 0;
static int verbose      = 0;
static const char *host = NULL;
static int uart_fd	= 0;
int debug = 0;

static int heartbeat_sec_cnt = 0;

#define MAGIC            0xDEADC0DE
#define MAGIC_LEN        4

#ifndef container_of
#define container_of(ptr, type, member)                             \
	({                                                              \
		const __typeof__(((type *) NULL)->member) *__mptr = (ptr);  \
		(type *) ((char *) __mptr - offsetof(type, member));        \
	})
#endif

static char g_p_mode[3][6] = {"test","toda","tof"};
static char g_p_role[2][6] = {"main","aux"};
static char g_p_sync[2][8] = {"disable","enable"};


//setting cmd buff, Will be used when no response is sent
static PARAM_CMD_t cmdbuf_param;
static PARAM_CMD_t cmdbuf_distance;
static PARAM_CMD_t cmdbuf_mode;
static PARAM_CMD_t cmdbuf_role;
static PARAM_CMD_t cmdbuf_clock;

static unsigned char g_uc_setparam_request = 0;


void buffer_offset(struct buffer *buf, int len)
{
	buf->len -= len;
	memmove(buf->data, buf->data + len, buf->len);
	// memset(buf->data + buf->len, 0x00, DATA_MAX_SIZE - buf->len);
}


void transDebug(uint8_t *data, uint16_t len)
{
	if(!debug)
		return;
	uint8_t execmd[1024] = {0};
	uint8_t buf[500*3+6] = {0};
	uint16_t i=0, index=0;
//		exec_command(execmd);
//	LOGI("**********************len:%d**********************\n", len);
	while(len)
	{
		sprintf(&buf[i],"%02X ", data[index]);
		i += 3;
		if(i>=180) {
			//sprintf(&buf[i], "index:%d", index);
			//LOGI("%s", buf);
			
			sprintf(execmd, "logger %s", buf);
			exec_command(execmd);
			i=0;
		}
		len--;
		index++;
	}
	if(i) {
		//LOGI("%s", buf);
		sprintf(execmd, "logger %s", buf);
		exec_command(execmd);
	}

}

void transHexDebug(uint8_t *data, uint16_t len)
{
	uint8_t execmd[1024] = {0};
	uint8_t buf[500*3+6] = {0};
	uint16_t i=0, index=0;
//		exec_command(execmd);
//	LOGI("**********************len:%d**********************\n", len);
	while(len)
	{
		sprintf(&buf[i],"%02X ", data[index]);
		i += 3;
		if(i>=180) {
			//sprintf(&buf[i], "index:%d", index);
			//LOGI("%s", buf);
			
			sprintf(execmd, "logger %s", buf);
			exec_command(execmd);
			i=0;
		}
		len--;
		index++;
	}
	if(i) {
		//LOGI("%s", buf);
		sprintf(execmd, "logger %s", buf);
		exec_command(execmd);
	}

}


int parse_header(struct parser *parser, int *abort)
{
	/* Too little data to parse */
	if (parser->buf.len < MSG_HEADER_SIZE) {
		*abort = 1;
		return 0;
	}

	uint8_t *pdata = parser->buf.data;

	parser->msg.magic = htonl(*(uint32_t *)pdata);
	pdata += 4;
	if (parser->msg.magic != MAGIC) {
		/* Cannot match magic */
		return -1;
	}
	if(debug)
		ULOG_INFO("0x%x == 0x%x", parser->msg.magic, MAGIC);

	parser->msg.length = htonl(*(uint32_t *)pdata);
	pdata += 4;
	if(debug)
		ULOG_INFO("%d > %d", parser->msg.length, PAYLOAD_MAX_SIZE);
	if (parser->msg.length > PAYLOAD_MAX_SIZE) {
		/* Message too long */
		return -1;
	}

	buffer_offset(&(parser->buf), MSG_HEADER_SIZE);

	return 0;
}

void parse_body(struct parser *parser, int *abort)
{
	if(debug)
		ULOG_INFO("%d > %d", parser->buf.len, parser->msg.length);
	/* Too little data to parse */
	if (parser->buf.len < parser->msg.length) {
		*abort = 1;
		return;
	}

	memcpy(parser->msg.payload, parser->buf.data, parser->msg.length);

	buffer_offset(&(parser->buf), parser->msg.length);
	
	return;
}

static int usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options]\n"
		"Options:\n"
		"    -H <server_host>   Host name or IP address of your remote server\n"
		"    -P <server_port>   Port number of your remote server\n"
		"    -u <val>           Communication protocol. UDP or TCP\n"
		"    -b <baudrate>      UART Baud rate (115200 is default)\n"
		"    -d <databits>      UART Databits, value can be 5 to 8\n"
		"    -s <stopbits>      Stop bits, value can be 1 or 2\n"
		"    -t <tty>           Device name(/dev/ttyS0, etc)\n"
		"    -p <parity>        Use parity bit (odd, even, none)\n"
		"    -v                 Verbose mode\n"
		"    -e                 Debug mode\n"
		"\n", prog);
	return 1;
}

static void print_bytes(uint8_t *buf, uint16_t size)
{
    int i = 0;
	if(debug) {
		transDebug(buf, size);
	}
	
}



static void sock_reconnect(struct uloop_timeout *timeout)
{
	sock_ctx_t *sock_ctx = container_of(timeout, sock_ctx_t, retry);
	assert(sock_ctx);

	static bool connected = false;

	if(debug)
		ULOG_INFO("sock_reconnect check %d", heartbeat_sec_cnt);
	

	if(!connected)
	{
		/*没有连接则创建连接*/
		sock_ctx->forwarder.fd = usock((use_udp) ? (USOCK_UDP) : (USOCK_TCP), host, usock_port(port));
	}
	
	if (sock_ctx->forwarder.fd < 0) {
		/*文件描述符无效, 设置为未连接*/
		ULOG_ERR("Failed to connect %s:%d, reason: %m\n", host, port);
		connected = false;
	} else {

		/*心跳包检测是否超时*/
		if(heartbeat_sec_cnt > HEARTBEAT_PACKET_INTERVAL) {
			/*	*/
			heartbeat_sec_cnt = 0;
			
			ULOG_ERR("Long time no heartbeat packet received, reconnect !");
			uloop_fd_delete(&sock_ctx->forwarder);
			close(sock_ctx->forwarder.fd);
			sock_ctx->forwarder.fd = -1;
			//uloop_timeout_set(&(sock_ctx->retry), 1000);
			//return;
		}

		/*文件描述符有效但状态为未连接,设置为已连接,监听描述符*/
		if(!connected)
		{
			//if(debug)
				ULOG_INFO("Connected to %s:%d\n", host, port);
			uloop_fd_add(&(sock_ctx->forwarder), ULOOP_READ);
			connected = true;
		}
	}
	uloop_timeout_set(&(sock_ctx->retry), 1000);
}


#if TEST_SOCK_CODE

static void test_sock_reconnect(struct uloop_timeout *timeout)
{
	sock_ctx_t *sock_ctx = container_of(timeout, sock_ctx_t, retry);
	assert(sock_ctx);

	sock_ctx->forwarder.fd = usock((use_udp) ? (USOCK_UDP) : (USOCK_TCP), "114.116.25.143", usock_port(7000));
	if (sock_ctx->forwarder.fd < 0) {
		//ULOG_ERR("Udp test: Failed to connect %s:%d, reason: %m\n", "114.116.25.143", 7000);
		uloop_timeout_set(&(sock_ctx->retry), 1000);
	} else {
		//ULOG_INFO("Udp test: Connected to %s:%d\n", "114.116.25.143", 7000);
		uloop_fd_add(&(sock_ctx->forwarder), ULOOP_READ);
	}
}

#endif

/**************************************************************
@function:      void stpOptionParam(char *option,char *buf)
@description:  	set config file /etc/config/stp
@param:         option: option  buf:set value
@return:        none
**************************************************************/
void stpOptionParam(char *option,char *buf)
{
	//Application context
	struct uci_context * ctx = uci_alloc_context();
	//configuration parameter
	struct uci_ptr ptr ={
	    .package = "stp",
	    .section = "cfg01ab7c",
	    .option = option,
	    .value = buf,
	};
	//write
	uci_set(ctx,&ptr);
	//commit
	uci_commit(ctx, &ptr.p, false);
	uci_unload(ctx,ptr.p);
	//free context
	uci_free_context(ctx);
}


/**************************************************************
@function:      char stpGetIndex(char *buf)
@description:  	get Gets the length of the set distance reply parameter
@param:         buf:parameter ,such as:23.35(m)
@return:        length
***************************************************************/
char stpGetIndex(char *buf)
{
	int i;
	for(i=0;i<24;i++)
	{
		if(buf[i]==')')
			return i+1;
	}
	return 0;
}

void stpUbusSendMg(uint8_t *event, uint8_t *top, uint8_t *message)
{
	if(event==NULL)
		return;
	uint8_t reply[512];
	memset(reply, 0, sizeof(reply));
	sprintf(reply, "ubus send %s \"{ '%s':'%s' }\"", event, top, message);
	exec_command(reply);
}


/**************************************************************
@function:      void stpSocketDataParser(char *buf)
@description:  	Parses whether the data is a configuration parameter response
@param:         buf:data
@return:        none
***************************************************************/
void stpSocketDataParser(char *buf)
{
	char param_buf[40];
	static char param_all[500]={0};
	memset(param_buf, 0, sizeof(param_buf));
	if(strlen(param_all)>sizeof(param_all)) {
		memset(param_all, 0, sizeof(param_all));
	}
	//getparameter cmd response
	if(strncmp(&buf[2], "IMEI", 4)==0)
	{
		g_uc_setparam_request &= ~(1<<STP_GET_PARAM);
		memcpy(param_buf, &buf[7], 12);

		//prooCmpMac(param_buf);
		//modify web print
		//stpOptionParam("IMEI", param_buf);
		//stpOptionParam("reply", "get parameters success");
		memset(param_all, 0, sizeof(param_all));
		sprintf(&param_all[strlen(param_all)], " imei:%s", param_buf);
		if(debug)
			ULOG_INFO("%s %d", param_all, strlen(param_all));
		//stpUbusSendMg("param", "imei", param_buf);
		//ULOG_INFO("%s", buf);
	}
	else if(strncmp(&buf[2], "distance", 8)==0)
	{
		//getparameter cmd response
		if(buf[10]==':')
		{
			/* 
			distance:200.00 (m)
			distance: 22.00 (m)
			*/
			sscanf(buf, "\r\ndistance:%s (m)\r\n", param_buf);
			//ULOG_INFO("%s %d", buf, stpGetIndex(&buf[12]));
			//memcpy(param_buf, &buf[12], stpGetIndex(&buf[12]));
			//ULOG_INFO("%s %d", param_all, strlen(param_all));
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " dis:%s (m)", param_buf);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
		}
		//setdistance cmd response
		else
		{
			g_uc_setparam_request &= ~(1<<STP_CFG_DISTANCE);
			memcpy(param_buf, &buf[13], stpGetIndex(&buf[13]));
			//modify web print
			//stpOptionParam("getdistance", param_buf);
			//notify ubus
			//stpOptionParam("reply", "set distance success");
			stpUbusSendMg("dis", "success", param_buf);
		}
	}
	else if(strncmp(&buf[2], "mode", 4)==0)
	{
		if(buf[6]==':')
		{
			uint8_t mode[10];
			memset(mode, 0, sizeof(mode));
			mode[0] = buf[7];
			//stpUbusSendMg("param", "mode", mode);
			//stpOptionParam("getmode", g_p_mode[buf[7]-'0']);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " mode:%s", mode);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
		}
		//setmode mode response
		else
		{
			g_uc_setparam_request &= ~(1<<STP_CFG_MODE);
			//modify web print
			//stpOptionParam("getmode", g_p_mode[buf[9]-'0']);
			//notify ubus, web
			uint8_t mode[10];
			memset(mode, 0, sizeof(mode));
			mode[0] = buf[9];
			stpUbusSendMg("mode", "success", mode);
		}
	} else if(strncmp(&buf[2], "role", 4)==0)
	{
		if(buf[6]==':') {
			//notify ubus, web
			uint8_t role[10];
			memset(role, 0, sizeof(role));
			role[0] = buf[7];
			//stpUbusSendMg("param", "role", role);
			//stpOptionParam("getrole", g_p_role[buf[7]-'0']);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " role:%s", role);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
		} else { //setrole cmd response
			g_uc_setparam_request &= ~(1<<STP_CFG_ROLE);
			//modify web print
			//stpOptionParam("getrole", g_p_role[buf[9]-'0']);
			//notify ubus, web
			uint8_t role[10];
			memset(role, 0, sizeof(role));
			role[0] = buf[9];
			stpUbusSendMg("role", "success", role);
		}
 	} else if(strncmp(&buf[2], "clock synchronize status", 24)==0) {
 		if(buf[26]==':') {
			//stpOptionParam("getclockstatus", g_p_sync[buf[27]-'0']);
			//notify ubus, web
			uint8_t clock[10];
			memset(clock, 0, sizeof(clock));
			clock[0] = buf[27];
			//stpUbusSendMg("param", "clock", clock);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " clock:%s", clock);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
			//ULOG_INFO("%s", param_all);
			//stpUbusSendMg("param", "success", param_all);
			//memset(param_all, 0, sizeof(param_all));
		} else {		//setclocksyn cmd response
			//clear flag
			g_uc_setparam_request &= ~(1<<STP_CFG_CLOCK);
			//modify web print
			//stpOptionParam("getclockstatus", g_p_sync[buf[29]-'0']);
			//notify ubus, web
			uint8_t clock[10];
			memset(clock, 0, sizeof(clock));
			clock[0] = buf[29];
			stpUbusSendMg("clock", "success", clock);
		}
	} else if(strncmp(&buf[2], "dimension", 9)==0) {
		if(buf[11]==':') {
			//notify ubus, web
			uint8_t dimension[10];
			memset(dimension, 0, sizeof(dimension));
			dimension[0] = buf[12];
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " dimension:%s", dimension);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
		} else { //setrole cmd response
			g_uc_setparam_request &= ~(1<<STP_CFG_ROLE);
			//notify ubus, web
			uint8_t dimension[10];
			memset(dimension, 0, sizeof(dimension));
			dimension[0] = buf[14];
			stpUbusSendMg("dimension", "success", dimension);
		}
  	} else if(strncmp(&buf[2], "panid", 5)==0) {
		//getparameter cmd response
		if(buf[7]==':') {
			memcpy(param_buf, &buf[8], stpGetIndex(&buf[8]));
			//modify web print
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " panid:%s", param_buf);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
		} else {	//setdistance cmd response
			g_uc_setparam_request &= ~(1<<STP_CFG_DISTANCE);
			memset(param_buf, 0, sizeof(param_buf));
			memcpy(param_buf, &buf[10], 2);
			if(debug)
				ULOG_INFO(param_buf);
			//notify ubus
			//stpOptionParam("reply", "set distance success");
			stpUbusSendMg("panid", "success", param_buf);
		}
	} else if(strncmp(&buf[2], "version:", 8)==0) {
		
			uint8_t version[30];
			memset(version, 0, sizeof(version));
			sscanf(&buf[2], "version:%s\r\n", &version);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			sprintf(&param_all[strlen(param_all)], " version:%s", version);
			if(debug)
				ULOG_INFO("%s %d", param_all, strlen(param_all));
			//ULOG_INFO("%s", buf);
			ULOG_INFO("%s", param_all);
			stpUbusSendMg("param", "success", param_all);
			memset(param_all, 0, sizeof(param_all));
			
	} else if(strncmp(&buf[0], "upgrade enter", strlen("upgrade enter"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_ENTER;
	} else if(strncmp(&buf[0], "upgrade ready", strlen("upgrade ready"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_READY;
	} else if(strncmp(&buf[0], "upgrade go", strlen("upgrade go"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_READY;
	} else if(strncmp(&buf[0], "upgrade error", strlen("upgrade error"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_ERROR;
	} else if(strncmp(&buf[0], "upgrade success", strlen("upgrade success"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_SUCCES;
	} else if(strncmp(&buf[0], "upgrade timeout", strlen("upgrade timeout"))==0) {
		g_upgrade_status_t.status = UPGRADE_STATUS_TIMEOUT;
		g_upgrade_status_t.step = 0;
	}  else if(strncmp(&buf[0], "upgade illegal, password error!", strlen("upgade illegal, password error!"))==0) {//
		ULOG_INFO("upgrade fail, cmd error! [固件升级失败,致命错误!]");
		//g_Stp_env_t.flag_upgrade_mode=UPGRADE_STOP;
		g_upgrade_status_t.step = 0;
		g_upgrade_status_t.status = UPGRADE_STATUS_NONE;
		//unlink(FILE_OUTPUT);
	}
}

void sock_forwarder_cb(struct uloop_fd *u, unsigned int events)
{
	sock_ctx_t *sock_ctx = container_of(u, sock_ctx_t, forwarder);
	assert(sock_ctx);

	char buf[DATA_MAX_SIZE] = {0};

	if (u->eof) {
		uloop_fd_delete(u);
		close(sock_ctx->forwarder.fd);
		sock_ctx->forwarder.fd = -1;
		uloop_timeout_set(&(sock_ctx->retry), 1000);
		return;
	}

	ssize_t r = read(u->fd, buf, DATA_MAX_SIZE);
	if (r < 0) {
		ULOG_ERR("Socket got something wrong! Try to reconnect...\n");
		uloop_fd_delete(u);
		close(sock_ctx->forwarder.fd);
		sock_ctx->forwarder.fd = -1;
		uloop_timeout_set(&(sock_ctx->retry), 1000);
		return;
	} else {
		if (verbose) {
			if(debug)
				ULOG_INFO("Data received from socket: \n");
			print_bytes(buf, r);
		}
	}

	if(g_Stp_env_t.flag_upgrade_mode==UPGRADE_STOP) {
		if(upgradeCheck(buf, r)==false) {
			if(strncmp(buf, "settime", 7)==0) {
				if(debug) 
					ULOG_INFO("get settime packet");
			}
			/* 只要收到数据说明通信链路正常 */
			heartbeat_sec_cnt = 0;
			
			if(strstr(buf, "heart ok")) {
				ULOG_INFO("get 'heart ok' packet, The packet will be drop.");
			} else {
				ssize_t w = write(sock_ctx->uart->forwarder.fd, buf, r);
				if (w < 0) {
					ULOG_ERR("Uart got something wrong!\n");
					return;
				} else {
					if (verbose) {
						if(debug)
							ULOG_INFO("Data sent to uart: \n");
						print_bytes(buf, w);
					}
				}
			}
		}
	} else {	//提示系统正在升级
		ssize_t w = write(sock_ctx->forwarder.fd, "The system is being upgraded, please waiting.", strlen("The system is being upgraded, please waiting"));
		if (w < 0) {
			ULOG_ERR("The system is being upgraded, but network is disconnected\n");
			return;
		} else {
			if (verbose) {
				if(debug)
					ULOG_INFO("Data sent to socket: \n");
				print_bytes(buf, w);
			}
		}
	}

}

unsigned char memfind(unsigned char *data, unsigned int data_length, unsigned char *findData, unsigned int findData_length)
{
	for (int i = 0; i <= data_length - findData_length; i++)
	{
		if (memcmp(&data[i], findData, findData_length) == 0)
		{
			return 1;
		}
	}
	return 0;
}

unsigned char memCheckErrorFrame(unsigned char *data, unsigned int length)
{
	const unsigned char errorFrame[4] = {0xde, 0xad, 0xc0, 0xde};
	if (length < sizeof(errorFrame))
	{
		return 0;
	}
	return memfind(data, length, errorFrame, sizeof(errorFrame));
}


void uart_forwarder_cb(struct uloop_fd *u, unsigned int events)
{
	uart_ctx_t *uart_ctx = container_of(u, uart_ctx_t, forwarder);
	assert(uart_ctx);

	ssize_t r = 0;
	ssize_t w = 0;

	r = read(u->fd, uart_ctx->parser.buf.data + uart_ctx->parser.buf.len, DATA_MAX_SIZE - uart_ctx->parser.buf.len);
	if (r > 0) {
		if(debug)
			ULOG_INFO("uart read %d bytes:", r);
		transDebug(uart_ctx->parser.buf.data + uart_ctx->parser.buf.len, r);
		uart_ctx->parser.buf.len += r;
	}
	
	
	while (uart_ctx->parser.buf.len > 0) {
		int abort = 0;
		if (uart_ctx->parser.opcode == OP_ON_HEADER) {
			if (parse_header(&(uart_ctx->parser), &abort) < 0) {
				/* Parse failed, will remove one byte  */
				buffer_offset(&(uart_ctx->parser.buf), 1);
			} else {
				if (abort)
					break;
				uart_ctx->parser.opcode = OP_ON_BODY;
			}
		}
		
		if (uart_ctx->parser.opcode == OP_ON_BODY) {
			parse_body(&(uart_ctx->parser), &abort);
			if (abort)
				break;

			//Parser
			stpSocketDataParser(uart_ctx->parser.msg.payload);

			uart_ctx->parser.opcode = OP_ON_HEADER;

			if(g_Stp_env_t.flag_upgrade_mode==UPGRADE_START) {
				uint8_t hex_buf[128] = {0};
				uint16_t hex_size=0;
				if(g_upgrade_status_t.status == UPGRADE_STATUS_NONE) {	//开始升级
 					g_upgrade_status_t.status = UPGRADE_STATUS_NUM;			//send upgrade 83689216, 等待响应
 					memcpy(&hex_buf[0], "upgrade 83689216\r\n", strlen("upgrade 83689216\r\n"));
					hex_size = strlen("upgrade 83689216\r\n");
					if(debug)
						ULOG_INFO("upgrade 83689216\r\n");
				} else if(g_upgrade_status_t.status == UPGRADE_STATUS_ENTER) {	//响应后状态为enter
					g_upgrade_status_t.step = 0;
					g_upgrade_status_t.status = UPGRADE_STATUS_START;				//send upgrade start, 等待响应
 					memcpy(&hex_buf[0], "upgrade start\r\n", strlen("upgrade start\r\n"));
					hex_size = strlen("upgrade start\r\n");
					if(debug)
						ULOG_INFO("upgrade start\r\n");
				} else if(g_upgrade_status_t.status == UPGRADE_STATUS_READY) {		//已经准备, 发送数据, 等待接收完成响应
					hex_size = upgradeUwbFirmwareStep(&hex_buf[0]);
					g_upgrade_status_t.status = UPGRADE_STATUS_GO;
					if(debug)
						ULOG_INFO("upgrade go\r\n");
				} else if(g_upgrade_status_t.status == UPGRADE_STATUS_ERROR)	{	//升级失败,需要回到enter重新升级
					if(debug)
						ULOG_INFO("upgrade error, re-upgrade it wating...");
					g_upgrade_status_t.step = 0;
				} else if(g_upgrade_status_t.status == UPGRADE_STATUS_SUCCES) {		//升级成功
					ULOG_INFO("upgrade firmware success! [固件升级成功!]");
					g_Stp_env_t.flag_upgrade_mode=UPGRADE_STOP;
					g_upgrade_status_t.step = 0;
					g_upgrade_status_t.status = UPGRADE_STATUS_NONE;
					unlink(FILE_OUTPUT);
					w = write(uart_ctx->sock->forwarder.fd, uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
				}
				w = write(uart_ctx->forwarder.fd, hex_buf, hex_size);
				if (w < 0) {
					
				}
			}

			//memCheck(uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
			//需要在这里判断包尾0x0d 0x0a,避免异常数据帧
			if(uart_ctx->parser.msg.payload[uart_ctx->parser.msg.length-1]==0x0a && 
				uart_ctx->parser.msg.payload[uart_ctx->parser.msg.length-2]==0x0d &&
				!memCheckErrorFrame(uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length))
			{
				
				//添加该判断防止在udp重连时不断发送失败重连
				if(uart_ctx->sock->forwarder.fd>0) {
					w = write(uart_ctx->sock->forwarder.fd, uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
					if (w < 0) {
						ULOG_ERR("Send got something wrong!  Please restart the host udp tool or check the network\n");
						uloop_fd_delete(&uart_ctx->sock->forwarder);
						close(uart_ctx->sock->forwarder.fd);
						uart_ctx->sock->forwarder.fd = -1;
						uloop_timeout_set(&(uart_ctx->sock->retry), 1000);
						return;
					} else {
						if (verbose) {
							if(debug)
								ULOG_INFO("Data sent to socket: \n");
							print_bytes(uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
						}
					}
				}
				
#if TEST_SOCK_CODE
				//发送数据
				if(uart_ctx->test_sock->forwarder.fd>0) {
					
					w = write(uart_ctx->test_sock->forwarder.fd, uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
					if (w < 0) {
						//ULOG_ERR("Send got something wrong!  Please restart the host udp tool or check the network\n");
						uloop_fd_delete(&uart_ctx->test_sock->forwarder);
						close(uart_ctx->test_sock->forwarder.fd);
						uart_ctx->test_sock->forwarder.fd = -1;
						uloop_timeout_set(&(uart_ctx->test_sock->retry), 1000);
						return;
					} else {
						if (verbose) {
							//ULOG_INFO("Data sent to test socket: \n");
							//print_bytes(uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
						}
					}
				}
#endif

			}
			else
			{
				ULOG_INFO("[error] Found abnormal packets, The packet will be dropped.");
				transHexDebug(uart_ctx->parser.msg.payload, uart_ctx->parser.msg.length);
			}
		}
	}
}

void uart_shutdown(uart_ctx_t *ctx)
{
	if (ctx->forwarder.registered) {
		uloop_fd_delete(&(ctx->forwarder));
	}

	if (ctx->forwarder.fd > 0) {
		close(ctx->forwarder.fd);
	}
}

void sock_shutdown(sock_ctx_t *ctx)
{
	if (ctx->forwarder.registered) {
		uloop_fd_delete(&(ctx->forwarder));
	}

	if (ctx->forwarder.fd > 0) {
		close(ctx->forwarder.fd);
	}
}


void test_sock_shutdown(sock_ctx_t *ctx)
{
	if (ctx->forwarder.registered) {
		uloop_fd_delete(&(ctx->forwarder));
	}

	if (ctx->forwarder.fd > 0) {
		close(ctx->forwarder.fd);
	}
}


/**************************************************************
@function:      void timer_handle(void)
@description:  	Timer handle,If timeout, it will be resend,A timeout of 3 times will 			notify web,1 times 3s
@param:         none
@return:        none
**************************************************************/
void timer_handle(void)
{
	heartbeat_sec_cnt++;

	//counts, 1s once
	static char count_get=0;
	static char count_dis=0;
	static char count_mode=0;
	static char count_role=0;
	static char count_clock=0;
	int w;
	if(g_uc_setparam_request & 1<<STP_GET_PARAM) {
		
		if(count_get>20) {	
			//clear flag
			g_uc_setparam_request &= ~(1<<STP_GET_PARAM);
			//notify web
			//stpOptionParam("reply", "Get parameters fail ! Please check connect !");
			
			//exec_command("ubus send param \"{ 'param':'fail' }\"");
			stpUbusSendMg("param", "param", "fail");
		} else {
			count_get++;
			//5 seconds,resend
			if(count_get%5==0) {
				//w = write(uart_fd, "getparameters\r\n", 15);
				//ULOG_INFO("reget %s", cmdbuf_param.cmdbuff);
			}
		}
	}
 	else {
		count_get = 0;
	}
	if(g_uc_setparam_request & 1<<STP_CFG_DISTANCE) {
		//if 1+2 ，then setting fail
		if(count_dis>=12) {
			stpUbusSendMg("dis", "dis", "fail");
			//notify web
			//stpOptionParam("reply", "set distance fail");
			//clear flag
			g_uc_setparam_request &= ~(1<<STP_CFG_DISTANCE);
		} else {
			count_dis++;
			//3 seconds,resend
			if(count_dis%3==0)
				w = write(uart_fd, cmdbuf_distance.cmdbuff, cmdbuf_distance.size);
		}
	} else {
		count_dis = 0;
	}
	if(g_uc_setparam_request & 1<<STP_CFG_MODE) {
		//if 1+2 ，then setting fail
		if(count_mode>=12) {
			stpUbusSendMg("mode", "mode", "fail");
			//stpOptionParam("reply", "set mode fail");
			g_uc_setparam_request &= ~(1<<STP_CFG_MODE);
		} else {
			count_mode++;
			if(count_mode%3==0)
				w = write(uart_fd, cmdbuf_mode.cmdbuff, cmdbuf_mode.size);
		}
	} else {
		count_mode = 0;
	}
	if(g_uc_setparam_request & 1<<STP_CFG_ROLE)
	{
		//if 1+2 ，then setting fail
		if(count_role>=12)
		{
			stpUbusSendMg("role", "role", "fail");
			//stpOptionParam("reply", "set role fail");
			g_uc_setparam_request &= ~(1<<STP_CFG_ROLE);
		}
		else
		{
			count_role++;
			if(count_role%3==0)
				w = write(uart_fd, cmdbuf_role.cmdbuff, cmdbuf_role.size);
		}
	}
	else
	{
		count_role = 0;
	}
	if(g_uc_setparam_request & 1<<STP_CFG_CLOCK)
	{
		//if 1+2 ，then setting fail
		if(count_clock>=12)
		{
			stpUbusSendMg("clock", "clock", "fail");
			//stpOptionParam("reply", "set clock synchronize status fail");
			g_uc_setparam_request &= ~(1<<STP_CFG_CLOCK);
		}
		else
		{
			count_clock++;
			if(count_clock%3==0)
				w = write(uart_fd, cmdbuf_clock.cmdbuff, cmdbuf_clock.size);
		}
	}
	else
	{
		count_clock = 0;
	}


}

/**************************************************************
@function:      void *pthreadConfigUwb(void *arg)
@description:  	thread function, To receive pipeline data,Sent to the uart
@param:         arg: not used
@return:        not used
**************************************************************/
void *pthreadConfigUwb(void *arg)
{
	char buff[100] = {0};
	int fifo_fd=0;
	ssize_t w_size = 0;
	ssize_t r_size;

	//remove pipe file
	unlink("mnt/uwb");
	//create pipe file
	if(mkfifo("mnt/uwb", 0644) == -1)
	{
		ULOG_ERR("create fifo fail!\n");
		return 0;
	}

	//create timer, Used to check if the configuration was successful
	timer_t timer;
	struct sigevent evp;
	struct itimerspec ts;
	
	int ret;
	evp.sigev_value.sival_ptr = &timer;
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = SIGUSR1;
	signal(SIGUSR1, timer_handle);
	ret = timer_create(CLOCK_REALTIME, &evp, &timer);
 	if( ret ) {
		perror("timer_create");
	}
	ts.it_interval.tv_sec = 1;
	ts.it_interval.tv_nsec = 0;
	ts.it_value.tv_sec = 1;
	ts.it_value.tv_nsec = 0;
	ret = timer_settime(timer, 0, &ts, NULL);
	if( ret )
		perror("timer_settime");
	
	

	while(1)
	{
		if(!fifo_fd) {
			//opened in read only mode, it will block in here
			fifo_fd = open("mnt/uwb", O_RDONLY);
			if(fifo_fd < 0) {
				//if open fail ,
				ULOG_ERR("fifo file broken!\n");
				unlink("mnt/uwb");
 				if(mkfifo("mnt/uwb", 0644) == -1) {
					ULOG_ERR("create fifo fail!\n");
					return 0;
				}
				fifo_fd=0;
				//close(fifo_fd);
				continue;
			}
		}
		memset(buff, 0, sizeof(buff));
		r_size = read(fifo_fd, buff, sizeof(buff));
		if(r_size < 0) {
			if(debug)
				ULOG_INFO("read fifo fail");
			fifo_fd = 0;
			continue;
		} else if(strncmp(buff, "getprogress", 11)==0) {
			if(debug)
				ULOG_INFO("get upgraded progess");
			uint8_t progress[60] = {0};
			sprintf(progress, "%d.%d", upgrade_file_size, upgrade_read_size);
			sleep(1);
			stpUbusSendMg("getprogress", "progress", progress);
		}
		
 		if(g_Stp_env_t.flag_upgrade_mode == UPGRADE_STOP) {
			//Sent to the uwb uart
			w_size = write(uart_fd, buff, r_size);
			if (w_size < 0) {
				ULOG_ERR("cfg param: Send got something wrong!\n");
			}
			if(w_size>0)
			//if(1)
			{
				if(strncmp(buff, "getparameters", 13)==0)
				{
					//flag
					//The fetch parameter is not reissued, so it is not cached
					g_uc_setparam_request |= 1<<STP_GET_PARAM;
					w_size = write(uart_fd, buff, r_size);
					//cached buff
					memcpy(cmdbuf_param.cmdbuff, buff, r_size);
					cmdbuf_param.size = r_size;
					ULOG_INFO("web getparameters");
				}
				else if(strncmp(buff, "setdistance", 11)==0)
				{
					//test_url();
					//flag
					g_uc_setparam_request |= 1<<STP_CFG_DISTANCE;
					//cached buff
					memcpy(cmdbuf_distance.cmdbuff, buff, r_size);
					cmdbuf_distance.size = r_size;
				}
				else if(strncmp(buff, "setmode", 7)==0)
				{
					g_uc_setparam_request |= 1<<STP_CFG_MODE;
					memcpy(cmdbuf_mode.cmdbuff, buff, r_size);
					cmdbuf_mode.size = r_size;
				}
				else if(strncmp(buff, "setrole", 7)==0)
				{
					g_uc_setparam_request |= 1<<STP_CFG_ROLE;
					memcpy(cmdbuf_role.cmdbuff, buff, r_size);
					cmdbuf_role.size = r_size;
				}
				else if(strncmp(buff, "setclocksyn", 11)==0)
				{
					g_uc_setparam_request |= 1<<STP_CFG_CLOCK;
					memcpy(cmdbuf_clock.cmdbuff, buff, r_size);
					cmdbuf_clock.size = r_size;
				} else if(strncmp(buff, "upgrade 83689216", 16)==0) {
					if(debug)
						ULOG_INFO("rec web upgrade cmd");
					upgrade_file_size = upgradeFileSize(FILE_OUTPUT);
					upgrade_read_size = 0;
					g_Stp_env_t.flag_upgrade_mode = UPGRADE_START;
					g_upgrade_status_t.status = UPGRADE_STATUS_NUM;
				}
			}
 		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int ch;
	int stopbits = 1;
	int databits = 8;
	int baudrate = 115200;
	
	pthread_t ntid;
	const char *parity = "none";
	const char *device = "/dev/ttyS1";

	sock_ctx_t sock_ctx = {0};
	uart_ctx_t uart_ctx = {0};
#if TEST_SOCK_CODE
	sock_ctx_t test_sock_ctx = {0};
#endif
	uart_ctx_t uwb_config = {0};

	//create thread 
	pthread_create(&ntid, NULL, pthreadConfigUwb, NULL);
	
	verbose = 1;

//	for(int i=0; i<argc; i++)
//	{
//		ULOG_INFO("%d: %s", i, argv[i]);
//	}

	while ((ch = getopt(argc, argv, "e:H:P:u:d:p:b:s")) != -1) {
//		ULOG_INFO("opt %c: %s", ch, optarg);
		switch (ch) {
		/*case 'v':
			verbose = 1;
			break;*/
		case 'H':
			host = optarg;
			break;
		case 'P':
			port = atoi(optarg);
			break;
		case 'u':
			if (!strncasecmp(optarg, "TCP", 3)) {
				use_udp = 0;
			} else {
				use_udp = 1;
			}
			break;
		case 'p':
			parity = optarg;
			break;
		case 'b':
			baudrate = atoi(optarg);
			break;
		case 's':
//			ULOG_INFO("stopbits %s", optarg);
			stopbits = atoi(optarg);
			break;
		/*case 'd':
			databits = atoi(optarg);
			break;*/
		case 'd':
//			ULOG_INFO("device %s", optarg);
			device = optarg;
			break;
		case 'e':
//			ULOG_INFO("debug %s", optarg);
			debug = atoi(optarg);
			break;
		default:
			return usage(*argv);
		}
	}

	if (!host || !port) {
		return usage(*argv);
	}

//	ULOG_INFO("log %d", debug);
	uloop_init();

	uart_ctx.sock = &sock_ctx;
#if TEST_SOCK_CODE
	uart_ctx.test_sock = &test_sock_ctx;
#endif
	uart_ctx.forwarder.cb = uart_forwarder_cb;
	uart_ctx.forwarder.fd = open_tty(device, baudrate, databits, stopbits, parity);
	uart_fd = uart_ctx.forwarder.fd;
	if (uart_ctx.forwarder.fd < 0) {
		goto out;
	}
	uloop_fd_add(&(uart_ctx.forwarder), ULOOP_READ);

	sock_ctx.uart = &uart_ctx;
	sock_ctx.retry.cb = sock_reconnect;
	sock_ctx.forwarder.cb = sock_forwarder_cb;
#if TEST_SOCK_CODE
	test_sock_ctx.uart = &uart_ctx;
	test_sock_ctx.retry.cb = test_sock_reconnect;
	test_sock_ctx.forwarder.cb = sock_forwarder_cb;
	uloop_timeout_set(&(test_sock_ctx.retry), 1000);
#endif
	uloop_timeout_set(&(sock_ctx.retry), 1000);
	uloop_run();

out:
	sock_shutdown(&sock_ctx);
	uart_shutdown(&uart_ctx);
#if TEST_SOCK_CODE
	test_sock_shutdown(&test_sock_ctx);
#endif
	uloop_done();

	return 0;
}

