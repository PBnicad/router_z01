#include "guart.h"
#include "transcoder.h"
//#include "rdss_uart.h"
#include "ble_record.h"

//extern void msg_publisher_add(const char *msg, int msg_id, int try_count);


#define KEEPALIVE_MSEC             2000
#define TTY_TIMER_MSEC             1000 /* !!!Cannot less than 1000 */
#define WIFI_CHECK_MSEC            20000
#define WIFI_GPRS_MSEC			   1800000


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

int fd_uuart = 0;

static void bytes_dbg(uint8_t *buf, uint16_t size)
{
	int i = 0;

	for (i = 0; i < size; i++)
		LOGI("%02X", buf[i]);

	LOGI("\n");
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
		LOGI("Packet send to bluetooth: \n");
		bytes_dbg(buf, index);
	}

	ssize_t s = writen(fd, buf, index);
	if (s < 0) {
		LOGE("Fail to send data to serial COM, packet will be drop\n");
	}
}

/*
 * state: false, offline; true, online
 * */
void cloud_state_report(int fd, bool state)
{
	uint8_t data;
	uint32_t utc_time = (uint32_t)time(NULL);

	if (state) {
		/* Online */
		data = 0x00;
		packet_send(fd, utc_time, TYPE_NOTIFY, &data, 1);
		// uloop_fd_add(&(tty_ctx->fd), ULOOP_READ | ULOOP_EDGE_TRIGGER);
	} else {
		/* Offline */
		data = 0x01;
		packet_send(fd, utc_time, TYPE_NOTIFY, &data, 1);
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
		LOGI("start download\n");
		snprintf(dl_url, URL_MAX_LEN, "http://192.168.8.207:8668/download/%.*s", notify->fname_len, notify->payload);
	} else {
		snprintf(dl_url, URL_MAX_LEN, "http://%s:%d/downLoad/version?filename=%.*s", url, notify->port, notify->fname_len, notify->payload);
	}
	

	LOGI("Downloading Bluetooth firmware from: %s\n", dl_url);

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
			LOGE("Fail to open bluetooth firmware\n");
			return -1;
		}

		firmware_len = fread(firmware_buf, 1, st.st_size, fp);
		if (firmware_len <= 0) {
			LOGE("Fail to read bluetooth to memory\n");
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
		LOGI("TTY upgrade notify recv LAYOUT 3: \n");
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
		LOGI("Fail to download firmware, upgrade failure\n");

		data[1] = 0x01;
		packet_send(tty_ctx->fd.fd, tty_ctx->pkt.timestamp, TYPE_UPGRADE_RESP, data, 2);

		firmware_status = FIRMWARE_IDLE;
	} else {
		LOGI("Success to download firmware, upgrade state change to READIED\n");

		data[1] = 0x00;
		packet_send(tty_ctx->fd.fd, tty_ctx->pkt.timestamp, TYPE_UPGRADE_RESP, data, 2);

		firmware_status = FIRMWARE_READIED;
	}
}

static void tty_upgrade_recv_reps(tty_ctx_t *tty_ctx)
{
	upgrade_process_reps_t *reps = (upgrade_process_reps_t *)(tty_ctx->pkt.payload + P_UPGRADE_HDR_LEN);

	LOGI("TTY upgrade response recv LAYOUT 3: \n");
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
			LOGE("Unknown response: %02x\n", reps->status);
			break;
	}
}

static void tty_upgrade_recv(tty_ctx_t *tty_ctx)
{
	fw_upgrade_t *upgrade = (fw_upgrade_t *)(tty_ctx->pkt.payload);

	if (verbose) {
		LOGI("TTY upgrade recv LAYOUT 2: \n");
		LOGI("%02x\n", upgrade->step);
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
			LOGE("Unsupport step: %02x\n", upgrade->step);
			break;
	}
}

extern uint8_t toBuff_test_flag;
extern uint8_t toBuff_test[2048];
void handle_tty_packet_test(tty_ctx_t *tty_ctx)
{
	if(toBuff_test_flag) {
		uint8_t testdata[5] = {1,14,14,31,1};
		if(DEBUG) {
			transDebug(testdata, 5);
		}
		
		mqttMessageSend(tty_ctx->mosq, toBuff_test, strlen(toBuff_test));
		if(DEBUG)
			LOGI("ble pack pub message:%s", toBuff_test);
		toBuff_test_flag = 0;
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
			LOGI("Handle packet in local: \n");
			bytes_dbg(buf, len);
		}

		switch (func_code) {
			case FUNC_UPGRADE:	//0x09
				tty_upgrade_recv(tty_ctx);
				break;
			case FUNC_NITIFY:	//0x40
//				/* tty_state_report_recv(tty_ctx); */
				LOGI("Received cloud_state_report response\n");
//				message_cache[CACHE_CLOUD_STATE].enable = false;
				break;
			default:
				LOGE("Unsupport function code: %02x\n", func_code);
				break;
		}
	} else {
		uint8_t deal_buf[SOCK_PACKET_MAX_LEN] = {0x00};
		uint32_t deal_len = 0;

		uint8_t *pdata = deal_buf;

		memset(deal_buf, 0x00, SOCK_PACKET_MAX_LEN);

		/* Copy data */
		*(uint32_t *)pdata = htonl(SOCK_MAGIC);
		pdata += 4;

		*(uint32_t *)pdata = htonl(len);
		pdata += 4;

		memcpy(pdata, buf, len);
		if(len > sizeof(buf)) {
			LOGE("guart buff size error: %d", __LINE__);
		}
		deal_len = len + 8;

		if (verbose) {
			LOGI("Forward packet to cloud: \n");
			bytes_dbg(deal_buf, deal_len);
		}

		
		uint8_t toBuff[2048];
		memset(toBuff, 0, sizeof(toBuff));
		//conversion data
		if(deal_len > sizeof(deal_buf)) {
			LOGE("deal buff size error: %d", __LINE__);
		}
		uint16_t buff_len = transToJson(deal_buf, deal_len, toBuff, sizeof(toBuff));

		mqttMessageSend(tty_ctx->mosq, toBuff, strlen(toBuff));
		//handle_tty_packet_test(tty_ctx);
		
	if(toBuff_test_flag) {
		uint8_t testdata[5] = {1,14,14,31,1};
		if(DEBUG) {
			transDebug(testdata, 5);
		}
		
		mqttMessageSend(tty_ctx->mosq, toBuff_test, strlen(toBuff_test));

		if(DEBUG)
			LOGI("ble pack pub message:%s", toBuff_test);
		toBuff_test_flag = 0;
		memset(toBuff_test, 0, sizeof(toBuff_test));
	}
		if(DEBUG)
			LOGI("pub message:%s", toBuff);
		//exec_logstring_command(toBuff);
		//exec_command()
	}
}

static void handle_tty_messages(tty_ctx_t *tty_ctx, uint8_t *data, uint16_t len)
{
	int i, idx;

	/* Only declare one time */
	static uint16_t crc = 0;
	static uint16_t count = 0;
	static int state = PARSER_IDLE_STATE;
	
	static uint8_t query_once_version = 1;
	if(query_once_version) {
		query_once_version = 0;
		uint8_t qbuf[20] = {0xAA, 0x57, 0x6D, 0xC0, 0x5D, 0x00, 0x00, 0x15, 0xA0, 0x33, 0x03, 0x00, 0x95, 0xD4, 0x8C, 0x00, 0x00, 0x6B, 0x05, 0x55};
		writen(fd_uuart, qbuf, 20);
	}

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
					LOGE("Received wrong length\n");
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
					LOGE("Received wrong payload\n");
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

extern struct mosq_config *g_mosq_config_t;

extern void handle_tty_json_messages(uint8_t *data, uint16_t data_len, tty_ctx_t *tty_ctx)
{
	static uint8_t rec_data_buff[1024];
	static uint16_t rec_data_buff_len = 0;

	static uint8_t left = 0;
	static uint8_t right = 0;
	
	for(uint16_t i=0; i<data_len; i++) {
		rec_data_buff[rec_data_buff_len] = data[i];
		rec_data_buff_len++;

		switch(data[i]) {
			case '{':
				left++;
				if(right!=0) {
					memset(rec_data_buff, 0, sizeof(rec_data_buff));
					rec_data_buff_len = 0;
					right = 0;
					left = 1;
					rec_data_buff[rec_data_buff_len] = data[i];
					rec_data_buff_len++;
				}
				break;
			case '}':
				right++;
				if(left<right) {
					//if(left==0) {
						memset(rec_data_buff, 0, rec_data_buff_len);
						left = 0;
						right = 0;
						rec_data_buff_len = 0;
					//}
				} else if(left==right) {
					mqttMessageSend(tty_ctx->mosq, rec_data_buff, strlen(rec_data_buff));
					
					memset(rec_data_buff, 0, sizeof(rec_data_buff));
					rec_data_buff_len = 0;
					right = 0;
					left = 0;

				}
				break;
			default:
				if(left==0) {
					memset(rec_data_buff, 0, rec_data_buff_len);
					left = 0;
					right = 0;
					rec_data_buff_len = 0;
				}
				break;
		}
		if(rec_data_buff_len>=sizeof(rec_data_buff)) {
			memset(rec_data_buff, 0, sizeof(rec_data_buff));
			rec_data_buff_len = 0;
			right = 0;
			left = 0;
		}
	}
}

static void tty_recv_cb(struct uloop_fd *fd, unsigned int events)
{
	uint8_t buf[BUF_SIZE] = {0};
	tty_ctx_t *tty_ctx = container_of(fd, tty_ctx_t, fd);

	ssize_t r = read(tty_ctx->fd.fd, buf, BUF_SIZE);
	if (r < 0) {
		LOGI("Fail to read message from serial COM\n");
		return;
	}

	if (verbose) {
		LOGI("TTY recv: \n");
		bytes_dbg(buf, r);
	}

	if(g_mosq_config_t->mode) {
		handle_tty_json_messages(buf, r, tty_ctx);
	} else {
		handle_tty_messages(tty_ctx, buf, r);
	}
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
			LOGI("Upgrade state: ready\n");
			/* Reset firmware variables */
			firmware_idx = 0;
			firmware_count = 0x0000;

			*(uint32_t *)buf = firmware_len;
			sleep(15);
			upgrade_packet_send(tty_ctx->fd.fd, UPGRADE_START, buf, 0x04);
			firmware_status = FIRMWARE_UPGRADING;
			break;
		case FIRMWARE_UPGRADING_NEXT:
			LOGI("Upgrade state: upgrading\n");
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
			LOGI("Upgrade state: upgraded\n");
			upgrade_packet_send(tty_ctx->fd.fd, UPGRADE_END, buf, 0x00);
			firmware_status = FIRMWARE_UPGRADING;
			break;
		case FIRMWARE_FAILURE:
			fail_time_c++;

			/* Retry after 3minute */
			// if (fail_time_c >= (3*60)) {
			if (fail_time_c >= (3*60*(1000/TTY_TIMER_MSEC))) {
				LOGI("Upgrade try again\n");
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
	ble_record_list_second_call();

	uloop_timeout_set(&(tty_ctx->timer), TTY_TIMER_MSEC);
}



/*****************************************************************************************************
@function:      static void MonitoringWifi_cb(struct uloop_timeout *t)
@description:   Detect wifi profile on a regular basis and report flow data on a regular basis
@param:         
@return:        none
******************************************************************************************************/
static void MonitoringWifi_cb(struct uloop_timeout *t)
{
	tty_ctx_t *tty_ctx = container_of(t, tty_ctx_t, MonitoringWifi);
	static uint8_t first = 1;
	const uint8_t times = WIFI_GPRS_MSEC/WIFI_CHECK_MSEC;
	static uint16_t count = times;
	
	count++;
	if(count>=times) {
		count = 0;
		uint8_t buf[512];
		memset(buf, 0, sizeof(buf));
		//report Network Statistics
		transNetworkStatistics(buf);
		mqttMessageSend(tty_ctx->mosq, buf, strlen(buf));
	}
	// For the first time to run
	if(first) {
		first = 0;
		FILE *fp = fopen("/etc/config/wireless", "r");
		fread(wifiinfo_buf, 1, sizeof(wifiinfo_buf), fp);
		fclose(fp);
	}
	uint8_t r_buf[1024];
	memset(r_buf, 0, sizeof(r_buf));
	
	FILE *fp = fopen("/etc/config/wireless", "r");
	ssize_t size = fread(r_buf, 1, sizeof(r_buf), fp);
	if(strncmp(wifiinfo_buf, r_buf, size)!=0) {
		memset(wifiinfo_buf, 0, sizeof(wifiinfo_buf));
		memcpy(wifiinfo_buf, r_buf, size);
		uint8_t json_buf[512];
		transWifiInfo(json_buf);
		
		mqttMessageSend(tty_ctx->mosq, json_buf, strlen(json_buf));
	}
	fclose(fp);
	uloop_timeout_set(&(tty_ctx->MonitoringWifi), WIFI_CHECK_MSEC);
}


static tty_ctx_t *new_tty(int fd)
{
	tty_ctx_t *tty_ctx = ss_malloc(sizeof(tty_ctx_t));
	memset(tty_ctx, 0, sizeof(tty_ctx_t));

	tty_ctx->buf = ss_malloc(sizeof(buffer_t));
	balloc(tty_ctx->buf, BUF_SIZE);

	tty_ctx->fd.fd = fd;
	tty_ctx->fd.cb = tty_recv_cb;

	tty_ctx->timer.cb = timer_cb;
	tty_ctx->MonitoringWifi.cb = MonitoringWifi_cb;
	fd_uuart = fd;
	
	return tty_ctx;
}

static void free_tty(tty_ctx_t *tty_ctx)
{
	if (tty_ctx->buf != NULL) {
		bfree(tty_ctx->buf);
		ss_free(tty_ctx->buf);
	}

	ss_free(tty_ctx);
}

void* mqttPthreadUart(void *arg)
{
	setlogmask(~(LOG_ERR | LOG_INFO | LOG_NOTICE | LOG_CRIT | LOG_ALERT | LOG_EMERG));
	LOGI("mqttPthreadUart");
	

	struct mosquitto *mosq = (struct mosquitto *)arg;

	int tty_fd = 0;

	char *device      = g_mosq_config_t->device; //"/dev/ttyS2";
	int baudrate      = 115200;

	tty_fd = open_tty(device, baudrate, false);
	if (tty_fd < 0) {
		LOGE("open tty fail");
		exit(EXIT_FAILURE);
	}

	uloop_init();
	//rdss_uart_init();

	tty_ctx_t *tty_ctx = new_tty(tty_fd);
	tty_ctx->mosq = mosq;

	uloop_fd_add(&(tty_ctx->fd), ULOOP_READ | ULOOP_EDGE_TRIGGER);

	if(!g_mosq_config_t->mode) {
		uloop_timeout_set(&(tty_ctx->timer), TTY_TIMER_MSEC);
		uloop_timeout_set(&(tty_ctx->MonitoringWifi), WIFI_CHECK_MSEC);
	}
	
	uloop_run();

out:
	free_tty(tty_ctx);

	uloop_done();
	
	pthread_exit(NULL);
}

void cosCreateThread(struct mosquitto *mosq)
{
	pthread_t ntid = 0;
	uint8_t err = pthread_create(&ntid, NULL, mqttPthreadUart, (void *)mosq);
	LOGI("thread mqtt");
	if (0 != err) {
		LOGI("can't creat thread: %s\n", strerror(err));
	}
	pthread_detach(ntid);
}


