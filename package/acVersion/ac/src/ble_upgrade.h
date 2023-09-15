#ifndef BLE_UPGRADE_H__
#define BLE_UPGRADE_H__

#include <stdbool.h>
#include "com.h"

	
enum {
	OTHER_ERR_SUCCESS = 0xF0,
	OTHER_ERR_404     = 0xF1,
	OTHER_ERR_BUSY    = 0xF2,
	OTHER_ERR_THREAD  = 0xFC,
	OTHER_ERR_MEM     = 0xFD,
	OTHER_ERR_TIMEOUT = 0xFE,
};

int upgradeBle(char *upgradeFile, MESSAGE_TYPE msg_type, void (*callback)(uint8_t *res_code, MESSAGE_TYPE msgType));

#endif  //BLE_UPGRADE_H__
