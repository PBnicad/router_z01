#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <mosquitto.h>
#include <json-c/json.h>
#include <libubox/list.h>
#include <libubox/utils.h>

#include "com.h"

#include "utils.h"
#include "shell.h"
#include "gjson.h"
#include "guci.h"
#include "actask.h"
#include "config_init.h"
#include "mqtt_topic.h"
#include "mqtt_callbacks.h"
#include "udp_network.h"

uint8_t protocol_version = 0x02;

#define AC_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define AC_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

unsigned char connect_flag = 0; //

ACCESS_CONTRL_CONFIG_t access_contrl_config;

struct mosquitto *mosq_config = NULL;

int main(int argc, char **argv)
{
	USE_SYSLOG("Access Contrl");
	AC_LOG_INFO("Access Contrl Start.")

	int rc = 0;

	configInit(&access_contrl_config, argc, argv);

	protocol_version = access_contrl_config.version;

	if (access_contrl_config.port < 1 || access_contrl_config.port > 65535) {
		AC_LOG_ERROR("Invalid port given: %d", access_contrl_config.port);
		exit(EXIT_FAILURE);
	}

	if (access_contrl_config.keepalive > 65535) {
		AC_LOG_ERROR("Invalid keepalive given: %d", access_contrl_config.keepalive);
		exit(EXIT_FAILURE);
	}
	uint8_t exe_buf[128];

	if(access_contrl_config.id == NULL) {
		char client_id[20] = {0};
		snprintf(client_id, sizeof(client_id), "ac%s", get_mac(false));
		
		memset(exe_buf, 0 ,sizeof(exe_buf));
		snprintf(exe_buf, sizeof(exe_buf), "uci set acmanage.@server[0].id=\"%s\"", client_id);
		exec_command(exe_buf);
		exec_command("uci commit acmanage");
		exec_command("/etc/init.d/acmanage restart");
	}
	if(access_contrl_config.pub == NULL) {
		char topic[128] = {0};
		snprintf(topic, sizeof(topic), "ac/cloud/%s", get_mac(false));
		publishTopicAppend(topic);
		
		memset(exe_buf, 0 ,sizeof(exe_buf));
		snprintf(exe_buf, sizeof(exe_buf), "uci add_list acmanage.@server[0].pub=\"%s\"", topic);
		exec_command(exe_buf);
		exec_command("uci commit acmanage");
	} else {
		char buffer[2048] = {0};
		strcpy(buffer, access_contrl_config.pub);
		char *topic=strtok(buffer," ");
		while(topic) {
			AC_LOG_INFO("parse pub topic %s", topic);
			publishTopicAppend(topic);
			topic=strtok(NULL," ");
		}
	}
	if(access_contrl_config.sub == NULL) {
		char topic[128] = {0};
		snprintf(topic, sizeof(topic), "ac/%s/cloud", get_mac(false));
		subscribeTopicAppend(topic);
		subscribeTopicAppend("ac/gateway/cloud");
		
		memset(exe_buf, 0 ,sizeof(exe_buf));
		snprintf(exe_buf, sizeof(exe_buf),"uci add_list acmanage.@server[0].sub=\"%s\"", topic);
		exec_command(exe_buf);
		exec_command("uci add_list acmanage.@server[0].sub=\"ac/gateway/cloud\"");
		exec_command("uci commit acmanage");
	} else {
		char buffer[2048] = {0};
		strcpy(buffer, access_contrl_config.sub);
		char *topic=strtok(buffer," ");
		while(topic) {
			AC_LOG_INFO("parse sub topic %s", topic);
			subscribeTopicAppend(topic);
			topic=strtok(NULL," ");
		}
	}

	mosquitto_lib_init();

	mosq_config = mosquitto_new(access_contrl_config.id, false, &access_contrl_config);
	if (!mosq_config) {
		switch (errno) {
		case ENOMEM:
			AC_LOG_ERROR("Out of memory");
			break;
		case EINVAL:
			AC_LOG_ERROR("Invalid id and/or clean_session");
			break;
		}

		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	if (access_contrl_config.username && 
		mosquitto_username_pw_set(mosq_config, access_contrl_config.username, access_contrl_config.password)) {
		AC_LOG_ERROR("Problem setting username and password");

		mosquitto_destroy(mosq_config);
		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	/* Create a thread to deal with timer tasks */
	
	udpPthreadCreate();

	mosquitto_connect_callback_set(mosq_config, connect_callback);
	mosquitto_subscribe_callback_set(mosq_config, subscribe_callback);
	mosquitto_message_callback_set(mosq_config, message_callback);
	mosquitto_publish_callback_set(mosq_config, publish_callback);
	mosquitto_disconnect_callback_set(mosq_config, disconnect_callback);

	AC_LOG_INFO("Connecting to %s:%d[%s]", access_contrl_config.host, access_contrl_config.port, access_contrl_config.id);
	mosquitto_connect(mosq_config, access_contrl_config.host, access_contrl_config.port, access_contrl_config.keepalive);


	rc = mosquitto_loop_forever(mosq_config, -1, 1);
	if (rc) {
		AC_LOG_ERROR("mosq_config error:%s", mosquitto_strerror(rc));
	}

	mosquitto_destroy(mosq_config);
	mosquitto_lib_cleanup();

	return rc;
}
