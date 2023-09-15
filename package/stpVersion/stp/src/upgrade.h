#ifndef	__UPGRADE_H__
#define	__UPGRADE_H__


//#include <libubox/ulog.h>
#include <libubox/usock.h>
#include <libubox/uloop.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <libubox/ulog.h>
#include <pthread.h>

#include <curl/curl.h>



#include "shell.h"
#include "config.h"

#define	FILE_OUTPUT 	"/tmp/uwb_firmware.hex"

typedef	enum UPGRADE_STATUS_n {
	UPGRADE_STATUS_NONE,
	UPGRADE_STATUS_NUM,
	UPGRADE_STATUS_ENTER,
	UPGRADE_STATUS_START,
	UPGRADE_STATUS_READY,
	UPGRADE_STATUS_GO,
	UPGRADE_STATUS_TIMEOUT,
	UPGRADE_STATUS_ERROR,
	UPGRADE_STATUS_SUCCES,
}UPGRADE_STATUS_n;

typedef struct {
	uint16_t step;
	UPGRADE_STATUS_n status;
}UPGRADE_STEP_STATUS_t;

extern UPGRADE_STEP_STATUS_t g_upgrade_status_t;


uint16_t upgradeUwbFirmwareStep(uint8_t *hex_buf);
uint8_t upgradeTransToHex(uint8_t *buf, uint16_t len, uint8_t *buf_bin);
int upgradeFileSize(char* filename);


bool upgradeCheck(uint8_t *buf, uint16_t len);

extern uint32_t upgrade_file_size;
extern uint32_t upgrade_read_size;




#endif

