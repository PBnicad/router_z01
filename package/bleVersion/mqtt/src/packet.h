#ifndef _PACKET_H
#define _PACKET_H

#include <inttypes.h>

#include "utils.h"

#define P_START_LEN 17
#define P_END_LEN 3

#define P_UPGRADE_HDR_LEN 1

#define PACKET_HEAD 0x2A
#define PACKET_TAIL 0xD5

#define PACKET_MAX_LEN 360 /* MAX packet length cannot more than 300 */
#define PAYLOAD_MAX_LEN 340 /* Fixed length is 20Bytes */

/* Function code */
#define FUNC_UPGRADE 0x09
#define FUNC_NITIFY 0x40

#define SENSOR_TO_GATEWAY 0x01
#define GATEWAY_TO_SENSOR 0x03

/* Type code */
#define TYPE_UPGRADE 0xB009
#define TYPE_UPGRADE_RESP 0xD009

#define TYPE_NOTIFY 0xB040

#define UPGRADE_START 0x00
#define UPGRADE_PROGRESS 0x01
#define UPGRADE_END 0x02
#define UPGRADE_CACEL 0x03

/* Layout 1 */
typedef struct cositea_packet {
	uint8_t header;
    uint32_t timestamp;
    uint8_t reserved;
    uint8_t version;
    uint16_t type;
    uint8_t mac[6];
	uint16_t len;
	uint8_t payload[PAYLOAD_MAX_LEN];
	uint16_t crc;
    uint8_t tail;
} __attribute__((packed)) packet_t;

/* Layout 2*/
/**
 * step =
 *        0x01: upgrade_notify
 *        0x03: upgrade_process
 */
typedef struct fw_upgrade {
	uint8_t step;
	uint8_t payload[];
} __attribute__((packed)) fw_upgrade_t;

/* Layout 3*/
/**
 * 
 * ---------------------------------------------------------
 * | type  | ver   | fw_size | port  | name_len| file_name |
 * | 1Byte | 2Btye |  4Byte  | 2Byte |  1Byte  |   nByte   |
 * ---------------------------------------------------------
 *
 */
typedef struct upgrade_notify {
	uint8_t fw_type;
	uint16_t version;
	uint32_t fw_size;
	uint16_t port;
	uint8_t fname_len;
	uint8_t payload[];
} __attribute__((packed)) upgrade_notify_t;

/* Layout 3*/
/**
 * 
 * -----------------------------------------
 * | status| total | count |  len  | data  |
 * | 1Byte | 2Btye | 2Byte | 1Byte | nByte |
 * -----------------------------------------
 *
 */
typedef struct upgrade_process {
	uint8_t status;
	uint16_t total;
	uint16_t count;
	uint8_t len;
	uint8_t payload[];
} __attribute__((packed)) upgrade_process_t;

/* Layout 3*/
/**
 * 
 * -------------------------
 * | status| count | flag  |
 * | 1Byte | 2Byte | 1Byte |
 * -------------------------
 *
 */
typedef struct upgrade_process_reps {
	uint8_t status;
	uint16_t count;
	uint8_t flag;
} __attribute__((packed)) upgrade_process_reps_t;

#endif
