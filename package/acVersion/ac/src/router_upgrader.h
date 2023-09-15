#ifndef ROUTER_UPGRADER_H__
#define ROUTER_UPGRADER_H__

#include <stdint.h>
#include <stdbool.h>
#include <mosquitto.h>
#include "com.h"

typedef enum {
	ROUTER_UPGRADER_404,
	ROUTER_UPGRADER_FERROR,   //error file
}UPGRADER_EVENT;

typedef struct {
	uint8_t *cmd;
	MESSAGE_TYPE type;
}TRANSUPBUF_t;


bool routerUpgradePthreadCreate(uint8_t *url, uint8_t keepSetting, MESSAGE_TYPE msg_type);

void routerUpgradeFailCallbackSet(void (*callback)(MESSAGE_TYPE msg_type, UPGRADER_EVENT status));

#endif //ROUTER_UPGRADER_H__

