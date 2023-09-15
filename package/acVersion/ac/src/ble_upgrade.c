#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/stat.h>   
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>

#include "ble_upgrade.h"
#include "upgrade_serial_uart.h"
#include "upgrade_serial.h"
#include "utils.h"
#include "shell.h"
#include "com.h"

#define UPGRADE_LOG(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define UPGRADE_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

char upgradeFileName[128] = {0};

void (*upgradeBleReplyHandler)(uint8_t *res_code, MESSAGE_TYPE msgType);

void *pthread_upgrade_ble_handler(void *p_context)
{
	MESSAGE_TYPE msgType = *(MESSAGE_TYPE*)p_context;
	ss_free(p_context);
	
	UPGRADE_LOG("start ");
	//停止运行MQTT服务
	char out_value[10] = {0};
	command_output("uci get cositea_mqtt.@server[0].enable", out_value);
	if(out_value[0]=='1') {
		exec_command("/etc/init.d/cositea_mqtt stop");
	}

	upgradeSerialUartInit("/dev/ttyS1");
							
	upgradeStart(upgradeFileName);
	
	int ping_cnt;
	for(ping_cnt = 1; ping_cnt < 15; ping_cnt++) {
		
		if(upgradeOnPingStatus()){
			
			if(ping_cnt%3==0) {
				upgardeDfuEnter();
			} else {
				upgradeRestart();
			}
		} else {
			break;
		}
		sleep(1);
	}
	
	if(ping_cnt == 15) {
		//timeout
		if(upgradeBleReplyHandler) {
			uint8_t err[2] = {0};
			err[0] = OTHER_ERR_TIMEOUT;
			upgradeBleReplyHandler(err, msgType);
		}
		goto clean;
	}
	
	while(1)
	{
		UPGRADE_LOG("upgrade Progress Percent %d%%.\n", upgradeProgressPercent());
		if(upgradeIsComplete()) {
			uint8_t *err = upgradeGetError();
			if(upgradeBleReplyHandler) {
				upgradeBleReplyHandler(err, msgType);
			}
			break;
		}
		sleep(1);
	}
	
	clean:
	upgradeSerialUartUnInit();
	
	//恢复运行MQTT服务
	if(out_value[0]=='1') {
		exec_command("/etc/init.d/cositea_mqtt start");
	}
	
	upgradeClean();
	
	UPGRADE_LOG("end\n");
	pthread_exit((void *)0);
}


int upgradeBle(char *upgradeFile, MESSAGE_TYPE msg_type, void (*callback)(uint8_t *res_code, MESSAGE_TYPE type))
{
	if(!upgradeIsIdle()) {
		uint8_t err[2] = {OTHER_ERR_BUSY, 0};
		callback(err, msg_type);
	}
	
	memset(upgradeFileName, 0, sizeof(upgradeFileName));
	strcpy(upgradeFileName, upgradeFile);
	
	upgradeBleReplyHandler = callback;
	
	MESSAGE_TYPE *msgType = (MESSAGE_TYPE *)ss_malloc(sizeof(MESSAGE_TYPE));
	if(!msgType) {
		uint8_t err[2] = {OTHER_ERR_MEM, 0};
		upgradeBleReplyHandler(err, msg_type);
		return -1;
	}
	memset(msgType, 0, sizeof(MESSAGE_TYPE));
	*msgType = msg_type;
	
	pthread_t ptd_id;
	uint8_t err = pthread_create(&ptd_id, NULL, pthread_upgrade_ble_handler, msgType);
	UPGRADE_LOG("ble upgrade thread create\n");
	if (0 != err) {
		UPGRADE_LOG("can't create thread: %s\n", strerror(err));
		uint8_t err[2] = {OTHER_ERR_THREAD, 0};
		upgradeBleReplyHandler(err, msg_type);
		return -1;
	}
	pthread_detach(ptd_id);
	
	
	return 0;
}


