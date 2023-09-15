#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <libubox/ulog.h>
#include <libubox/usock.h>
#include <libubox/uloop.h>

#include <json-c/json.h>

#include "utils.h"
#include "packet.h"
#include "serial.h"
#include "cositea.h"
#include "gjson.h"
#include "transcoder.h"

#define SOCK_PACKET_MAX_LEN (PACKET_MAX_LEN + 8)
#define SOCK_MAGIC 12345678

#define URL_MAX_LEN                218
#define CMD_MAX_LEN                512

#define BUF_SIZE                   2048
#define FIRMWARE_MAX_LEN           200
#define FIRMWARE_BUF_SIZE          (500 * 1024)

#define checksum(dat, crc)         (dat + crc)

enum {
	PARSER_IDLE_STATE,
	PARSER_TIMESTAMP_STATE,
	PARSER_RESERVED_STATE,
	PARSER_VERSION_STATE,
	PARSER_TYPE_STATE,
	PARSER_MAC_STATE,
	PARSER_LEN_STATE,
	PARSER_PAYLOAD_STATE,
	PARSER_CRC_STATE,
	PARSER_TAIL_STATE,
	PARSER_MAX
};

enum {
	FIRMWARE_IDLE,
	FIRMWARE_READIED,
	FIRMWARE_UPGRADING,
	FIRMWARE_UPGRADING_NEXT,
	FIRMWARE_SUCCESS,
	FIRMWARE_FAILURE,
	FIRMWARE_MAX
};

enum {
	CACHE_CLOUD_STATE,
	CACHE_UPGRADE,
	CACHE_MAX
};

typedef struct message {
	uint32_t timestamp;
	uint16_t type;
	uint8_t payload[PAYLOAD_MAX_LEN];
	uint16_t len;
} message_t;

typedef struct cache {
	bool enable;
	int retry;
	int count;
	message_t msg;
} cache_t;

typedef struct {
	uint64_t tx;
	uint64_t rx;
}FLOW_CUMULATIVE_t;

FLOW_CUMULATIVE_t flow_cumu_t = { 0, 0 };


static cache_t message_cache[CACHE_MAX];

static uint16_t firmware_len                      = 0;
static uint16_t firmware_idx                      = 0;
static uint16_t firmware_count                    = 0;
static uint16_t firmware_total                    = 0;
static uint8_t firmware_status                    = FIRMWARE_IDLE;
static uint8_t firmware_buf[FIRMWARE_BUF_SIZE]    = {0};
static uint8_t wifiinfo_buf[1024] = {0};

/* static char *url                   = "bracelet.cositea.com"; */
static char *url                   = "ac.imyfit.com";
static const char *firmware_file   = "/tmp/bluetooth_firmware.bin";

static int verbose = 0;

static char *help_string =
"uart2tcp\n"
"  usage:\n"
"    -B <baudrate>      Set baud speed for tty, if not specified, default is 115200.\n"
"    -d <device>        Specify serial port, default is /dev/ttyS1.\n"
"    -H <host>          Specify host for TCP server.\n"
"    -p <port>          Specify port for TCP server.\n"
"    -u <url>           Specify the firmware url to download.\n"
"    -f <pid_file>      The file path to store pid.\n"
"    [-v, --Verbose]    Verbose mode.\n"
"    [-h, --help]       Print this message.\n";

static void bytes_dbg(uint8_t *buf, uint16_t size)
{
	int i = 0;

	for (i = 0; i < size; i++)
		ULOG_INFO("%02X", buf[i]);

	ULOG_INFO("\n");
}

static void pkt_to_bytes(packet_t *pkt, uint8_t *buf, uint16_t *len)
{
	memcpy(buf, (uint8_t *)&(pkt->header), P_START_LEN);
	memcpy(buf + P_START_LEN, (uint8_t *)&(pkt->payload), pkt->len);
	memcpy(buf + P_START_LEN + pkt->len, (uint8_t *)&(pkt->crc), P_END_LEN);

	*len = P_START_LEN + pkt->len + P_END_LEN;
}

static void packet_store_cache(cache_t *c, uint32_t utc_time, uint16_t type, uint8_t *data, uint16_t len)
{
	if (!c || !data) {
		return;
	}

	memset(c, 0x00, sizeof(cache_t));
	c->enable = true;
	c->retry = 0;
	c->count = 0;
	c->msg.timestamp = utc_time;
	c->msg.type = type;
	memcpy(c->msg.payload, data, len);
	c->msg.len = len;
}

static void packet_send(int fd, uint32_t utc_time, uint16_t type, uint8_t *data, uint16_t len)
{
	uint16_t i = 0, index = 0;
    uint16_t crc = 0;

    uint8_t buf[PACKET_MAX_LEN];

    if (len > PACKET_MAX_LEN) {
		return;
    }

	/* Header */
    buf[index] = 0xAA;
    crc = checksum(buf[index++], 0);
    
	/* uint32_t utc_time = 0; */
	/* utc_time = (uint32_t)time(NULL); */

	/* Timestamp */
    buf[index] = (uint8_t)((utc_time >> 0) & 0xff);
    crc = checksum(buf[index++], crc);
    
    buf[index] = (uint8_t)((utc_time >> 8) & 0xff);
    crc = checksum(buf[index++], crc);
    
    buf[index] = (uint8_t)((utc_time >> 16) & 0xff);
    crc = checksum(buf[index++], crc);
    
    buf[index] = (uint8_t)((utc_time >> 24) & 0xff);
    crc = checksum(buf[index++], crc);
    
	/* Reserved */
    buf[index] = 0x00;
    crc = checksum(buf[index++], crc);
    
	/* Version */
    buf[index] = 0x00;
    crc = checksum(buf[index++], crc);
    
	/* Type */
    buf[index] = (uint8_t)((type >> 0) & 0xff);
    crc = checksum(buf[index++], crc);

    buf[index] = (uint8_t)((type >> 8) & 0xff);
    crc = checksum(buf[index++], crc);
    
	/* MAC */
	/*
	for (i = 0; i < 6; i++) {
		buf[index] = buf[index-1] + 0x11;
		crc = checksum(buf[index++], crc);
	}
	*/
    buf[index] = 0x12;
    crc = checksum(buf[index++], crc);
    
    buf[index] = 0x34;
    crc = checksum(buf[index++], crc);
    
    buf[index] = 0x56;
    crc = checksum(buf[index++], crc);
    
    buf[index] = 0x78;
    crc = checksum(buf[index++], crc);
    
    buf[index] = 0x9A;
    crc = checksum(buf[index++], crc);
    
    buf[index] = 0x0C;
    crc = checksum(buf[index++], crc);
    
	/* Length */
    buf[index] = (uint8_t)((len >> 0) & 0xff);
    crc = checksum(buf[index++], crc);
    
    buf[index] = (uint8_t)((len >> 8) & 0xff);
    crc = checksum(buf[index++], crc);

	/* Payload */
    for(i = 0; i < len; i++) {
        buf[index] = data[i];
        crc = checksum(buf[index++], crc);
    }

	/* CRC */
    buf[index++] = (uint8_t)((crc >> 0) & 0xff);
    buf[index++] = (uint8_t)((crc >> 8) & 0xff);

	/* Tail */
	buf[index++] = 0x55;

	if (verbose) {
		ULOG_INFO("Packet send to bluetooth: \n");
		bytes_dbg(buf, index);
	}

	ssize_t s = writen(fd, buf, index);
	if (s < 0) {
		ULOG_ERR("Fail to send data to serial COM, packet will be drop\n");
	}
}

/*
 * state: false, offline; true, online
 * */
static void cloud_state_report(tty_ctx_t *tty_ctx, bool state)
{
	uint8_t data;
	uint32_t utc_time = (uint32_t)time(NULL);

	if (state) {
		/* Online */
		data = 0x00;
		packet_send(tty_ctx->fd.fd, utc_time, TYPE_NOTIFY, &data, 1);
		// uloop_fd_add(&(tty_ctx->fd), ULOOP_READ | ULOOP_EDGE_TRIGGER);
	} else {
		/* Offline */
		data = 0x01;
		packet_send(tty_ctx->fd.fd, utc_time, TYPE_NOTIFY, &data, 1);
		// uloop_fd_delete(&(tty_ctx->fd));
	}

	/* Store */
	packet_store_cache(&message_cache[CACHE_CLOUD_STATE], utc_time, TYPE_NOTIFY, &data, 1);
}

static int firmware_download(upgrade_notify_t *notify)
{
	char cmd[CMD_MAX_LEN] = {0};
	char dl_url[URL_MAX_LEN] = {0};

	/*
	snprintf(dl_url, URL_MAX_LEN, "http://192.168.1.106:%d/downLoad/version?filename=%.*s", notify->port, notify->fname_len, notify->payload);
	*/
	//local test update 
	if(DEBUG) {
		ULOG_INFO("start download\n");
		snprintf(dl_url, URL_MAX_LEN, "http://192.168.8.207:8668/download/%.*s", notify->fname_len, notify->payload);
	} else {
		snprintf(dl_url, URL_MAX_LEN, "http://%s:%d/downLoad/version?filename=%.*s", url, notify->port, notify->fname_len, notify->payload);
	}
	

	ULOG_INFO("Downloading Bluetooth firmware from: %s\n", dl_url);

	unlink(firmware_file);
	if (download_file(dl_url, firmware_file, 5000) < 0) {
		return -1;
	}

	/* Vertify */
	struct stat st;
	if ((stat(firmware_file, &st) == 0) && (st.st_size == notify->fw_size)) {
		/* Copy firmware to buf */
		FILE *fp = NULL;

		/* Reset firmware various */
		firmware_len = 0;
		firmware_idx = 0;
		firmware_total = 0;
		memset(firmware_buf, 0x00, FIRMWARE_BUF_SIZE);

		fp = fopen(firmware_file, "rb");
		if (!fp) {
			ULOG_ERR("Fail to open bluetooth firmware\n");
			return -1;
		}

		firmware_len = fread(firmware_buf, 1, st.st_size, fp);
		if (firmware_len <= 0) {
			ULOG_ERR("Fail to read bluetooth to memory\n");
			fclose(fp);
			return -1;
		}

		firmware_total = ((firmware_len % FIRMWARE_MAX_LEN) == 0) ? (firmware_len / FIRMWARE_MAX_LEN) : (firmware_len / FIRMWARE_MAX_LEN + 1);

		if (fp)
			fclose(fp);
	} else {
		return -1;
	}

	return 0;
}

static void tty_upgrade_recv_notify(tty_ctx_t *tty_ctx)
{
	uint8_t data[2] = {0x00};

	upgrade_notify_t *notify = (upgrade_notify_t *)(tty_ctx->pkt.payload + P_UPGRADE_HDR_LEN);

	if (verbose) {
		ULOG_INFO("TTY upgrade notify recv LAYOUT 3: \n");
		bytes_dbg((uint8_t *)notify, tty_ctx->pkt.len - P_UPGRADE_HDR_LEN);
	}

	/* Received notify upgrade packet, response to bluetooth */
	/*
	 * -------------------------------
	 * | Step Type | Respone Content |
	 * -------------------------------
	 *
	 */
	data[0] = SENSOR_TO_GATEWAY;
	data[1] = 0x00;

	/* The timestamp of response packet should identical with original */
	// packet_send(tty_ctx->fd.fd, tty_ctx->pkt.timestamp, TYPE_UPGRADE_RESP, data, 2);

	if (firmware_download(notify) < 0) {
		ULOG_INFO("Fail to download firmware, upgrade failure\n");

		data[1] = 0x01;
		packet_send(tty_ctx->fd.fd, tty_ctx->pkt.timestamp, TYPE_UPGRADE_RESP, data, 2);

		firmware_status = FIRMWARE_IDLE;
	} else {
		ULOG_INFO("Success to download firmware, upgrade state change to READIED\n");

		data[1] = 0x00;
		packet_send(tty_ctx->fd.fd, tty_ctx->pkt.timestamp, TYPE_UPGRADE_RESP, data, 2);

		firmware_status = FIRMWARE_READIED;
	}
}

static void tty_upgrade_recv_reps(tty_ctx_t *tty_ctx)
{
	upgrade_process_reps_t *reps = (upgrade_process_reps_t *)(tty_ctx->pkt.payload + P_UPGRADE_HDR_LEN);

	ULOG_INFO("TTY upgrade response recv LAYOUT 3: \n");
	bytes_dbg((uint8_t *)reps, tty_ctx->pkt.len - P_UPGRADE_HDR_LEN);

	switch (reps->status) {
		case 0x00:
			/* Enter Upgrade */
			if (reps->flag == 0x00) {
				firmware_status = FIRMWARE_UPGRADING_NEXT;
			} else {
				firmware_status = FIRMWARE_FAILURE;
			}
			break;
		case 0x01:
			if (reps->flag == 0x00) {
				if (reps->count == firmware_total) {
					firmware_status = FIRMWARE_SUCCESS;
				} else {
					firmware_status = FIRMWARE_UPGRADING_NEXT;
				}
			} else {
				firmware_status = FIRMWARE_FAILURE;
			}
			break;
		case 0x02:
			if (reps->flag == 0x00) {
				/* Upgrade successfully */
				firmware_status = FIRMWARE_IDLE;
			} else {
				firmware_status = FIRMWARE_FAILURE;
			}
			break;
		case 0x03:
			break;
		default:
			ULOG_ERR("Unknown response: %02x\n", reps->status);
			break;
	}
}

static void tty_upgrade_recv(tty_ctx_t *tty_ctx)
{
	fw_upgrade_t *upgrade = (fw_upgrade_t *)(tty_ctx->pkt.payload);

	if (verbose) {
		ULOG_INFO("TTY upgrade recv LAYOUT 2: \n");
		ULOG_INFO("%02x\n", upgrade->step);
	}

	switch (upgrade->step) {
		case SENSOR_TO_GATEWAY:	//step 1
			tty_upgrade_recv_notify(tty_ctx);
			break;
		case GATEWAY_TO_SENSOR:	//step 3
			tty_upgrade_recv_reps(tty_ctx);
			message_cache[CACHE_UPGRADE].enable = false;
			break;
		default:
			ULOG_ERR("Unsupport step: %02x\n", upgrade->step);
			break;
	}
}

static void handle_tty_packet(tty_ctx_t *tty_ctx)
{
	uint16_t len = 0;
	uint8_t buf[PACKET_MAX_LEN] = {0x00};

	bool gateway = (tty_ctx->pkt.type >> 12) & 1U; /* bit12: direction, 0: to gateway, 1: to cloud */
	uint8_t func_code = (tty_ctx->pkt.type) & 0xff; /* bit0-bit7: fucntion code */

	/* Convert packet struct to byte */
	pkt_to_bytes(&(tty_ctx->pkt), buf, &len);

	if (gateway) {
		if (verbose) {
			ULOG_INFO("Handle packet in local: \n");
			bytes_dbg(buf, len);
		}

		switch (func_code) {
			case FUNC_UPGRADE:	//0x09
				tty_upgrade_recv(tty_ctx);
				break;
			case FUNC_NITIFY:	//0x40
				/* tty_state_report_recv(tty_ctx); */
				ULOG_INFO("Received cloud_state_report response\n");
				message_cache[CACHE_CLOUD_STATE].enable = false;
				break;
			default:
				ULOG_ERR("Unsupport function code: %02x\n", func_code);
				break;
		}
	} else {
		if (!tty_ctx->sock->connected) {
			return;
		}

		if (len > SOCK_PACKET_MAX_LEN) {
			ULOG_ERR("Packet too long, drop it\n");
			return;
		}

		uint8_t sock_buf[SOCK_PACKET_MAX_LEN] = {0x00};
		uint32_t sock_len = 0;

		uint8_t *pdata = sock_buf;

		memset(sock_buf, 0x00, SOCK_PACKET_MAX_LEN);

		/* Copy data */
		*(uint32_t *)pdata = htonl(SOCK_MAGIC);
		pdata += 4;

		*(uint32_t *)pdata = htonl(len);
		pdata += 4;

		memcpy(pdata, buf, len);
		sock_len = len + 8;

		if (verbose) {
			ULOG_INFO("Forward packet to cloud: \n");
			bytes_dbg(sock_buf, sock_len);
		}
		
//		ssize_t s = writen(tty_ctx->sock->fd.fd, sock_buf, sock_len);
//		if (s < 0) {
//			ULOG_ERR("Fail to forward packet to cloud, packet will be drop\n");
//		}
		//write to socket
		uint8_t toBuff[2048];
		memset(toBuff, 0, sizeof(toBuff));
		//conversion data
		uint16_t buff_len = transToJson(sock_buf, sock_len, toBuff, sizeof(toBuff));
		
		ssize_t s = writen(tty_ctx->sock->fd.fd, toBuff, buff_len);
		if (s < 0) {
			ULOG_ERR("Fail to forward packet to cloud, packet will be drop\n");
			sock_disconnected(tty_ctx->sock);
			uloop_timeout_set(&(tty_ctx->sock->keepalive), KEEPALIVE_MSEC);
		}

	}
}

static void handle_tty_messages(tty_ctx_t *tty_ctx, uint8_t *data, uint16_t len)
{
	int i, idx;

	/* Only declare one time */
	static uint16_t crc = 0;
	static uint16_t count = 0;
	static int state = PARSER_IDLE_STATE;

	for (idx = 0, i = 0; i < len; i++) {
		switch(state) {
			case PARSER_IDLE_STATE:
				/* Reset */
				crc = 0;
				count = 0;
				memset(&(tty_ctx->pkt), 0x00, sizeof(tty_ctx->pkt));
				if (data[i] == PACKET_HEAD) {
					tty_ctx->pkt.header = data[i];
					crc += data[i];
					idx = i;
					state = PARSER_TIMESTAMP_STATE;
				}
				break;
			case PARSER_TIMESTAMP_STATE:
				/* 4Bytes */
				tty_ctx->pkt.timestamp |= data[i] << (count << 3);
				crc += data[i];

				count++;
				if (count >= 4) {
					count = 0; /* Reset count */
					state = PARSER_RESERVED_STATE;
				}
				break;
			case PARSER_RESERVED_STATE:
				tty_ctx->pkt.reserved = data[i];
				crc += data[i];
				state = PARSER_VERSION_STATE;
				break;
			case PARSER_VERSION_STATE:
				tty_ctx->pkt.version= data[i];
				crc += data[i];
				state = PARSER_TYPE_STATE;
				break;
			case PARSER_TYPE_STATE:
				/* 2Bytes */
				tty_ctx->pkt.type |= data[i] << (count << 3);
				crc += data[i];

				count++;
				if (count >= 2) {
					count = 0; /* Reset count */
					state = PARSER_MAC_STATE;
				}
				break;
			case PARSER_MAC_STATE:
				tty_ctx->pkt.mac[count++] = data[i];
				crc += data[i];

				if (count >= 6) {
					count = 0; /* Reset count */
					state = PARSER_LEN_STATE;
				}
				break;
			case PARSER_LEN_STATE:
				/* 2Bytes */
				tty_ctx->pkt.len |= data[i] << (count << 3);
				crc += data[i];

				count++;
				if (count >= 2) {
					count = 0; /* Reset count */
					state = PARSER_PAYLOAD_STATE;
				}
				
				if (tty_ctx->pkt.len > PAYLOAD_MAX_LEN) {
					/* Error */
					ULOG_ERR("Received wrong length\n");
					i = idx;
					state = PARSER_IDLE_STATE;
				}
				break;
			case PARSER_PAYLOAD_STATE:
				tty_ctx->pkt.payload[count++] = data[i];
				crc += data[i];

				if (count == tty_ctx->pkt.len) {
					count = 0; /* Reset count */
					state = PARSER_CRC_STATE;
				}

				if (count > PAYLOAD_MAX_LEN) {
					ULOG_ERR("Received wrong payload\n");
					count = 0; /* Reset count */
					i = idx;
					state = PARSER_IDLE_STATE;
				}

				break;
			case PARSER_CRC_STATE:
				/* 2Bytes */
				tty_ctx->pkt.crc |= data[i] << (count << 3);

				count++;
				if (count >= 2) {
					count = 0; /* Reset count */
					state = PARSER_TAIL_STATE;
				}
				break;
			case PARSER_TAIL_STATE:
				if ((data[i] == PACKET_TAIL) && (tty_ctx->pkt.crc == crc)) {
					tty_ctx->pkt.tail = data[i];
					handle_tty_packet(tty_ctx);
					state = PARSER_IDLE_STATE;
				} else {
					/* Error */
					i = idx;
					state = PARSER_IDLE_STATE;
				}
				break;
		}
	}
}

static void sock_connected(sock_ctx_t *sock_ctx, int fd)
{
	sock_ctx->connected = 1;
	sock_ctx->fd.fd = fd;
	uloop_fd_add(&(sock_ctx->fd), ULOOP_READ | ULOOP_EDGE_TRIGGER);

	/* Notify bluetooth online */
	cloud_state_report(sock_ctx->tty, true);

	/* report router information */
	uint8_t buf_router_info[512];
	memset(buf_router_info, 0, sizeof(buf_router_info));
	transRouterInfo(buf_router_info);
	ssize_t s = writen(fd, buf_router_info, strlen(buf_router_info));
	if (s < 0) {
		ULOG_ERR("Fail to forward packet to cloud, packet will be drop\n");
		sock_disconnected(sock_ctx);
		uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
	}
}

extern void sock_disconnected(sock_ctx_t *sock_ctx)
{
	if (sock_ctx->connected) {
		sock_ctx->connected = 0;
		close(sock_ctx->fd.fd);
		uloop_fd_delete(&(sock_ctx->fd));

		/* Notify bluetooth offline */
		cloud_state_report(sock_ctx->tty, false);
	}
}

static void tty_recv_cb(struct uloop_fd *fd, unsigned int events)
{
	uint8_t buf[BUF_SIZE] = {0};
	tty_ctx_t *tty_ctx = container_of(fd, tty_ctx_t, fd);

	ssize_t r = read(tty_ctx->fd.fd, buf, BUF_SIZE);
	if (r < 0) {
		ULOG_INFO("Fail to read message from serial COM\n");
		return;
	}

	if (verbose) {
		ULOG_INFO("TTY recv: \n");
		bytes_dbg(buf, r);
	}

	handle_tty_messages(tty_ctx, buf, r);
}

static void upgrade_packet_send(int fd, uint8_t status, uint8_t *data, uint8_t len)
{
	fw_upgrade_t *upgrade = NULL;
	upgrade_process_t *pro = NULL;

	uint16_t size = (uint16_t)(sizeof(*upgrade) + sizeof(*pro) + len);
	uint32_t utc_time = (uint32_t)time(NULL);

	upgrade = (fw_upgrade_t *)malloc(size);
	if (!upgrade) {
		return;
	}

	upgrade->step = GATEWAY_TO_SENSOR;
	pro = (upgrade_process_t *)(upgrade->payload);
	pro->status = status;
	/* pro->total = (firmware_len + FIRMWARE_MAX_LEN / 2) / FIRMWARE_MAX_LEN; */
	pro->total = firmware_total;
	pro->count = firmware_count;
	pro->len = len;
	memcpy(pro->payload, data, len);

	packet_send(fd, utc_time, TYPE_UPGRADE, (uint8_t *)upgrade, size);

	/* Store */
	packet_store_cache(&message_cache[CACHE_UPGRADE], utc_time, TYPE_UPGRADE, (uint8_t *)upgrade, size);

	if (upgrade)
		free(upgrade);
}

static void retransmission_timer(tty_ctx_t *tty_ctx)
{
	int i = 0;

	for (i = 0; i < CACHE_MAX; i++) {
		if (!message_cache[i].enable) {
			continue;
		}

		if (message_cache[i].retry >= 3) {
			message_cache[i].enable = false;
		}

		/* Retransmission after 5 seconds */
		if ((++message_cache[i].count % (5*(1000/TTY_TIMER_MSEC))) == 0) {
			packet_send(tty_ctx->fd.fd, \
					message_cache[i].msg.timestamp, \
					message_cache[i].msg.type, \
					message_cache[i].msg.payload, \
					message_cache[i].msg.len);

			message_cache[i].retry++;
		}
	}
}

static void upgrade_timer(tty_ctx_t *tty_ctx)
{
	/* Increase */
	static int fail_time_c = 0;

	uint8_t len = 0;
	uint8_t buf[FIRMWARE_MAX_LEN] = {0x00};

	switch (firmware_status) {
		case FIRMWARE_READIED:
			ULOG_INFO("Upgrade state: ready\n");
			/* Reset firmware variables */
			firmware_idx = 0;
			firmware_count = 0x0000;

			*(uint32_t *)buf = firmware_len;
			sleep(15);
			upgrade_packet_send(tty_ctx->fd.fd, UPGRADE_START, buf, 0x04);
			firmware_status = FIRMWARE_UPGRADING;
			break;
		case FIRMWARE_UPGRADING_NEXT:
			ULOG_INFO("Upgrade state: upgrading\n");
			if ((firmware_len - firmware_idx) < FIRMWARE_MAX_LEN) {
				len = (uint8_t)(firmware_len - firmware_idx);
			} else {
				len = FIRMWARE_MAX_LEN;
			}

			/* n+1 */
			firmware_count++;

			memcpy(buf, firmware_buf + firmware_idx, len);
			upgrade_packet_send(tty_ctx->fd.fd, UPGRADE_PROGRESS, buf, len);
			firmware_idx += len;
			firmware_status = FIRMWARE_UPGRADING;
			break;
		case FIRMWARE_SUCCESS:
			ULOG_INFO("Upgrade state: upgraded\n");
			upgrade_packet_send(tty_ctx->fd.fd, UPGRADE_END, buf, 0x00);
			firmware_status = FIRMWARE_UPGRADING;
			break;
		case FIRMWARE_FAILURE:
			fail_time_c++;

			/* Retry after 3minute */
			// if (fail_time_c >= (3*60)) {
			if (fail_time_c >= (3*60*(1000/TTY_TIMER_MSEC))) {
				ULOG_INFO("Upgrade try again\n");
				fail_time_c = 0;
				firmware_status = FIRMWARE_READIED;
			}
			break;
		default:
			break;
	}
}

void timer_cb(struct uloop_timeout *t)
{
	tty_ctx_t *tty_ctx = container_of(t, tty_ctx_t, timer);

	retransmission_timer(tty_ctx);
	upgrade_timer(tty_ctx);

	uloop_timeout_set(&(tty_ctx->timer), TTY_TIMER_MSEC);
}


void sock_recv_cb(struct uloop_fd *fd, unsigned int events)
{
	sock_ctx_t *sock_ctx = container_of(fd, sock_ctx_t, fd);

	if (verbose)
		ULOG_INFO("Sock recv callback\n");

	if (sock_ctx->buf->len >= sock_ctx->buf->capacity) {
		/* Buffer overflow */
		return;
	}
	
	ssize_t r = recv(sock_ctx->fd.fd, sock_ctx->buf->data, PACKET_MAX_LEN, 0);
	if (r == 0) {
		ULOG_INFO("Connection closed\n");
		sock_disconnected(sock_ctx);
		uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
		return;
	} else if (r == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			ULOG_INFO("No data\n");
		} else {
			ULOG_INFO("Server recv\n");
			sock_disconnected(sock_ctx);
			uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
			return;
		}
	}
	else {
		
		//save deal with Recv data
		transRecvDownData(sock_ctx->buf->data, r, sock_ctx);
	}
#if 0
	if(DEBUG) {
		ssize_t r = recv(sock_ctx->fd.fd, sock_ctx->buf->data, PACKET_MAX_LEN, 0);
		ssize_t s;
		if(r > 100) {
			uint8_t hex[300];
			memset(hex, 0, 300);
			transDebug(sock_ctx->buf->data, r);
			uint16_t len = transJsonToHex(sock_ctx->buf->data, r, hex, sock_ctx);

			ULOG_INFO("%s:write %d bytes\n", sock_ctx->buf->data, len);
			s = writen(sock_ctx->tty->fd.fd, hex, len);
			if (s < 0) {
				ULOG_ERR("Fail to send message to serial COM\n");
				return;
			}
			transDebug(hex, len);
			return ;
		} else {
			s = writen(sock_ctx->tty->fd.fd, sock_ctx->buf->data, r);
			if (s < 0) {
				ULOG_ERR("Fail to send message to serial COM\n");
				return;
			}
		}
	} else {
		ssize_t r = recv(sock_ctx->fd.fd, sock_ctx->buf->data + sock_ctx->buf->len, sock_ctx->buf->capacity - sock_ctx->buf->len, 0);
		if (r == 0) {
			ULOG_INFO("Connection closed\n");
			sock_disconnected(sock_ctx);
			uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
			return;
		} else if (r == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				ULOG_INFO("No data\n");
			} else {
				ULOG_INFO("Server recv\n");
				sock_disconnected(sock_ctx);
				uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
				return;
			}
		}

		
		sock_ctx->buf->len += r;
		ssize_t s;
		s = writen(sock_ctx->tty->fd.fd, sock_ctx->buf->data, sock_ctx->buf->len);
		if (s < 0) {
			ULOG_ERR("Fail to send message to serial COM\n");
			return;
		}
		if (verbose) {
			ULOG_INFO("Sock sent: \n");
			bytes_dbg(sock_ctx->buf->data, s);
		}

		sock_ctx->buf->len -= s;
		sock_ctx->buf->idx += s;
		if (sock_ctx->buf->len == 0) {
			/* All data sent, reset buffer */
			sock_ctx->buf->idx = 0;
			memset(sock_ctx->buf->data, 0, sock_ctx->buf->capacity);
		} else {
			/* Remove sent data */
			memcpy(sock_ctx->buf->data, sock_ctx->buf->data + sock_ctx->buf->idx, sock_ctx->buf->len);
		}
	}
#endif
}

void keepalive_cb(struct uloop_timeout *t)
{
	sock_ctx_t *sock_ctx = container_of(t, sock_ctx_t, keepalive);
	int fd = 0;

	if (!sock_ctx->connected) {
		ULOG_INFO("Connecting to %s:%s\n", sock_ctx->host, sock_ctx->service);
		fd = usock(USOCK_TCP | USOCK_NOCLOEXEC | USOCK_IPV4ONLY, sock_ctx->host, sock_ctx->service);
		if (fd < 0) {
			ULOG_ERR("Unable to connect to the server\n");
			uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
		} else {
			ULOG_INFO("Connection is established\n");
			sock_connected(sock_ctx, fd);
			uloop_timeout_set(&(sock_ctx->MonitoringWifi), WIFI_CHECK_MSEC);
		}
	}
}

/*****************************************************************************************************
@function:      static void MonitoringWifi_cb(struct uloop_timeout *t)
@description:   Detect wifi profile on a regular basis and report flow data on a regular basis
@param:         
@return:        none
******************************************************************************************************/
static void MonitoringWifi_cb(struct uloop_timeout *t)
{
	sock_ctx_t *sock_ctx = container_of(t, sock_ctx_t, MonitoringWifi);
	static uint8_t first = 1;
	const uint8_t times = WIFI_GPRS_MSEC/WIFI_CHECK_MSEC;
	static uint16_t count = times;
	
	count++;
	if(count>=times) {
		count = 0;
		if(sock_ctx->connected) {
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));
			//report Network Statistics
			transNetworkStatistics(buf);
			ssize_t s = writen(sock_ctx->fd.fd, buf, strlen(buf));
			if(s < 0) {
				ULOG_ERR("Fail to forward packet to cloud, packet will be drop\n");
				sock_disconnected(sock_ctx);
				uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
			}
		}
	}
	// For the first time to run
	if(first) {
		first = 0;
		FILE *fp = fopen("/etc/config/wireless", "r");
		fread(wifiinfo_buf, 1, sizeof(wifiinfo_buf), fp);
		fclose(fp);
	}
	if(sock_ctx->connected) {
		uint8_t r_buf[1024];
		memset(r_buf, 0, sizeof(r_buf));
		
		FILE *fp = fopen("/etc/config/wireless", "r");
		ssize_t size = fread(r_buf, 1, sizeof(r_buf), fp);
		if(strncmp(wifiinfo_buf, r_buf, size)!=0) {
			memset(wifiinfo_buf, 0, sizeof(wifiinfo_buf));
			memcpy(wifiinfo_buf, r_buf, size);
			uint8_t json_buf[512];
			transWifiInfo(json_buf);
			ssize_t s = writen(sock_ctx->fd.fd, json_buf, strlen(json_buf));
			if(s < 0) {
				ULOG_ERR("Fail to forward packet to cloud, packet will be drop\n");
				sock_disconnected(sock_ctx);
				uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
			}
		}
		fclose(fp);
	}
	uloop_timeout_set(&(sock_ctx->MonitoringWifi), WIFI_CHECK_MSEC);
}

static tty_ctx_t *new_tty(int fd)
{
	tty_ctx_t *tty_ctx = ss_malloc(sizeof(tty_ctx_t));
	memset(tty_ctx, 0, sizeof(tty_ctx_t));

	tty_ctx->sock = ss_malloc(sizeof(sock_ctx_t));
	tty_ctx->buf = ss_malloc(sizeof(buffer_t));
	memset(tty_ctx->sock, 0, sizeof(sock_ctx_t));
	balloc(tty_ctx->buf, BUF_SIZE);

	tty_ctx->fd.fd = fd;
	tty_ctx->fd.cb = tty_recv_cb;

	tty_ctx->timer.cb = timer_cb;

	return tty_ctx;
}

static void free_tty(tty_ctx_t *tty_ctx)
{
	if (tty_ctx->buf != NULL) {
		bfree(tty_ctx->buf);
		ss_free(tty_ctx->buf);
	}

	// ss_free(tty_ctx->sock);
	ss_free(tty_ctx);
}

static sock_ctx_t *new_sock(char *host, char *service)
{
	sock_ctx_t *sock_ctx = ss_malloc(sizeof(sock_ctx_t));
	memset(sock_ctx, 0, sizeof(sock_ctx_t));

	sock_ctx->tty = ss_malloc(sizeof(tty_ctx_t));
	sock_ctx->buf = ss_malloc(sizeof(buffer_t));
	memset(sock_ctx->tty, 0, sizeof(tty_ctx_t));
	balloc(sock_ctx->buf, BUF_SIZE);

	sock_ctx->fd.cb = sock_recv_cb;

	sock_ctx->keepalive.cb = keepalive_cb;
	sock_ctx->MonitoringWifi.cb = MonitoringWifi_cb;

	sock_ctx->host = strdup(host);
	sock_ctx->service = strdup(service);
	sock_ctx->connected = 0;

	return sock_ctx;
}

static void free_sock(sock_ctx_t *sock_ctx)
{
	if (sock_ctx->buf != NULL) {
		bfree(sock_ctx->buf);
		ss_free(sock_ctx->buf);
	}

	// ss_free(sock_ctx->tty);
	ss_free(sock_ctx);
} 
int main(int argc, char **argv)
{
	int c;
	int rc = 0;

	int tty_fd = 0;

	char *host        = NULL;
	char *device      = "/dev/ttyS1";
	char *service     = NULL;
	char *pid_path    = NULL;

	int pid_flags     = 0;
	int baudrate      = 115200;

	static struct option long_options[] = {
		{ "device",   required_argument, NULL, 'd' },
		{ "baudrate", required_argument, NULL, 'B' },
		{ "host",     required_argument, NULL, 'H' },
		{ "port",     required_argument, NULL, 'p' },
		{ "url",      required_argument, NULL, 'u' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "help",     no_argument,       NULL, 'h' },
		{ 0,          0,                 NULL,  0  }
	};

	opterr = 0;

	while ((c = getopt_long(argc, argv, "d:B:H:p:u:f:vh",
					long_options, NULL)) != -1) {
		switch (c) {
			case 'd':
				device = optarg;
				break;
			case 'B':
				baudrate = atoi(optarg);
				break;
			case 'H':
				host = optarg;
				break;
			case 'p':
				service = optarg;
				break;
			case 'u':
				url = optarg;
				break;
			case 'f':
				pid_flags = 1;
				pid_path = optarg;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'h':
				fprintf(stderr, "%s", help_string); 
				exit(EXIT_SUCCESS);
				break;
			case '?':
				ULOG_ERR("Unrecognized option: %s", optarg);
				opterr = 1;
				break;
		}
	}

	if (opterr) {
		fprintf(stderr, "%s", help_string); 
		exit(EXIT_FAILURE);
	}

	if (!host || !service) {
		ULOG_ERR("No host or port specified\n");
		exit(EXIT_FAILURE);
	}

	if (pid_flags) {
		daemonize(pid_path);
	}

	/*
	tty_fd = usock(USOCK_TCP | USOCK_NOCLOEXEC | USOCK_IPV4ONLY, "localhost", "8888");
	if (tty_fd < 0) {
		ULOG_ERR("Fail to connect tty\n");
		exit(EXIT_FAILURE);
	}
	*/
	tty_fd = open_tty(device, baudrate, false);
	if (tty_fd < 0) {
		ULOG_ERR("open tty fail");
		exit(EXIT_FAILURE);
	}

	uloop_init();

	sock_ctx_t *sock_ctx = new_sock(host, service);
	tty_ctx_t *tty_ctx = new_tty(tty_fd);
	sock_ctx->tty = tty_ctx;
	tty_ctx->sock = sock_ctx;

	uloop_fd_add(&(tty_ctx->fd), ULOOP_READ | ULOOP_EDGE_TRIGGER);

	uloop_timeout_set(&(tty_ctx->timer), TTY_TIMER_MSEC);
	uloop_timeout_set(&(sock_ctx->keepalive), KEEPALIVE_MSEC);
	
	uloop_run();

out:
	free_tty(tty_ctx);
	free_sock(sock_ctx);

	uloop_done();

	return rc;
}
