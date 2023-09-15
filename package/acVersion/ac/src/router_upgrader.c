#include "router_upgrader.h"
#include <string.h>
#include <pthread.h>
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

#include <pthread.h>

//#include <libubox/ulog.h>
#include "com.h"
#include "utils.h"
#include "shell.h"

#define UPGRADER_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define UPGRADER_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

void (*upgrader_fail_handler)(MESSAGE_TYPE msg_type, UPGRADER_EVENT status);

volatile bool upgrade_busy = false;

/*****************************************************************************************************
@function:      void *pthreadUpgrade(void *arg)
@description:   update routeing firmware thread,It will wating 5 seconds, if update fail, will notify cloud
@param:         arg: struct TRANSSOCKBUF_t Pointer ;
@return:        0
******************************************************************************************************/
void *pthreadUpgrade(void *arg)
{
	TRANSUPBUF_t *cmd_t = NULL;
	cmd_t = (TRANSUPBUF_t *)arg;
	MESSAGE_TYPE msg_type = cmd_t->type;
	
	UPGRADER_LOG_INFO("waiting 15s");
	
	for(int i=0; i<5; i++) {
		UPGRADER_LOG_INFO("The router will start the update in %d seconds!", 5-i);
		sleep(1);
	}
	uint8_t value[512];
	memset(value, 0, sizeof(value));
	
	
	//UPGRADER_LOG_INFO(cmd_t->cmd);
	//exec_command(cmd_t->cmd);
	UPGRADER_LOG_INFO("%s",cmd_t->cmd);
	command_output(cmd_t->cmd, value);
	UPGRADER_LOG_INFO("%s",value);

	//if succesful, will be not run to here, in upgrading
	/* 404
	Downloading 'http://192.168.0.61:5001/test.1'
	Connecting to 192.168.0.61:5001
	HTTP error 404
	Image metadata not found
	Invalid image type.
	Image check 'platform_check_image' failed.
	*/
	/* file error
	Downloading 'http://192.168.0.61:5001/test.txt'
	Connecting to 192.168.0.61:5001
	Writing to '/tmp/sysupgrade.img'

	Connection reset prematurely
	Image metadata not found
	Invalid image type.
	Image check 'platform_check_image' failed.
	*/
	int err_code = 0;
	if(strstr(value, "HTTP error 404")==NULL) {
		err_code = msg_type, ROUTER_UPGRADER_404;
	} else {
		err_code = msg_type, ROUTER_UPGRADER_FERROR;
	}
	if(upgrader_fail_handler)
	{
		(*upgrader_fail_handler)(msg_type, ROUTER_UPGRADER_FERROR);
	}
	upgrade_busy = false;

	pthread_exit((void*)0);
}

void routerUpgradeFailCallbackSet(void (*callback)(MESSAGE_TYPE msg_type, UPGRADER_EVENT status))
{
	upgrader_fail_handler = callback;
}

bool routerUpgradePthreadCreate(uint8_t *url, uint8_t keepSetting, MESSAGE_TYPE msg_type)
{
	if(url==NULL || upgrade_busy) {
		return false;
	}
	
	static char upgrade_cmd[512] = {0};
	static TRANSUPBUF_t trans_cmd;
	trans_cmd.cmd = upgrade_cmd;
	trans_cmd.type = msg_type;
	
	memset(upgrade_cmd, 0, sizeof(upgrade_cmd));

	if(keepSetting) {
		sprintf(upgrade_cmd, "sysupgrade -n %s", url);
	} else 
		sprintf(upgrade_cmd, "sysupgrade %s", url);{
	}
	
	pthread_t ntid;
	int err = pthread_create(&ntid, NULL, pthreadUpgrade, &trans_cmd);
	if(err==0) {
		upgrade_busy = true;
		return true;
	} else {
		return false;
	}
}

