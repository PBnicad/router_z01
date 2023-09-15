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
#include <libubox/md5.h>
#include <libubox/list.h>
#include <libubox/utils.h>

#include <time.h>

#include "com.h"

#include "utils.h"
#include "shell.h"
#include "gjson.h"
#include "guci.h"
#include "transcoder.h"
#include "guart.h"

#define	MD5_LEN 33
#define SLEEP_TIME 5

uint8_t protocol_version = 0x02;

typedef struct {
	char *topic;
	void *next;
}TOPIC_NODE_t;

static TOPIC_NODE_t *g_subscribe_list = NULL;
static TOPIC_NODE_t *g_publish_list = NULL;

void topicAppend(TOPIC_NODE_t **head, char *topic)
{
	TOPIC_NODE_t *node = (TOPIC_NODE_t *)ss_malloc(sizeof(TOPIC_NODE_t));
	char *node_topic = (char *)ss_malloc(strlen(topic)+1);
	
	memset(node, 0, sizeof(TOPIC_NODE_t));
	memset(node_topic, 0, sizeof(strlen(topic)+1));

	strcpy(node_topic, topic);
	
	node->topic = node_topic;
	node->next = NULL;
	
	TOPIC_NODE_t *poll = (*head);
	if(poll == NULL) {
		*head = node;
	} else {
		while(poll->next)
		{
			poll = poll->next;
		}
		poll->next = node;
	}
}

void topicDelete(TOPIC_NODE_t **head, char *topic)
{
	TOPIC_NODE_t *poll = *head;

	if(strcmp(poll->topic, topic)==0) {
		*head = poll->next;
		ss_free(poll->topic);
		ss_free(poll);
		return;
	}

	while(poll->next) {
		TOPIC_NODE_t *node = poll->next;

		if(strcmp(node->topic, topic)==0) {
			poll->next = node->next;
			ss_free(node->topic);
			ss_free(node);
			break;
		}
		poll = poll->next;
	}
	
	
}

void viewTopic(TOPIC_NODE_t *head)
{
	printf("\nview list:\n");
	TOPIC_NODE_t *poll = head;
	while(poll)
	{
		printf("list %p %s\n", poll, poll->topic);
		poll = poll->next;
	}
}

void subscribeTopicAppend(char *topic)
{
	topicAppend(&g_subscribe_list, topic);
}
void subscribeTopicDelete(char *topic)
{
	topicDelete(&g_subscribe_list, topic);
}

void publishTopicAppend(char *topic)
{
	topicAppend(&g_publish_list, topic);
}
void publishTopicDelete(char *topic)
{
	topicDelete(&g_publish_list, topic);
}

static unsigned int g_system_cpu_arch = ARCH_MIPS;   //获取系统架构,在虚拟机运行时识别使用

unsigned char connect_flag = 0;


struct mosq_config *g_mosq_config_t;


enum {
	GETOPT_VAL_HELP,
};

void print_usage(void);
void init_config(struct mosq_config *cfg);
void config_cmdline_proc(struct mosq_config *cfg, int argc, char **argv);

void get_cpu_arch_init(void)		//获取系统架构
{
	FILE *std;
	float dy;
	char buf[1024];
	std = popen( "uname  -a", "r" );
	fread(buf, sizeof(char), sizeof(buf), std);
	if(strstr(buf,"i686") || strstr(buf,"i386") || strstr(buf,"x86-64"))
	{
		g_system_cpu_arch = ARCH_X86;
	}
	else if(strstr(buf,"ARM") || strstr(buf,"arm"))
	{
		g_system_cpu_arch = ARCH_ARM;
	}
	else if(strstr(buf,"mips"))
	{
		g_system_cpu_arch = ARCH_MIPS;
	}
	pclose(std);
}

void mqttReturns(int ret, char *info)
{
	switch(ret) {
		case MOSQ_ERR_SUCCESS:
			if(DEBUG)
				LOGI("Publish success: %s", info);
			break;
		case MOSQ_ERR_INVAL:
			LOGI("Publish 参数错误: %s", info);
			break;
		case MOSQ_ERR_NOMEM:
			LOGI("Publish 内存不足: %s", info);

			break;
		case MOSQ_ERR_NO_CONN:
			LOGI("Publish broker连接未建立: %s", info);
			break;
			
		case MOSQ_ERR_PROTOCOL:
			LOGI("Publish 协议错误: %s", info);
			break;
			
		case MOSQ_ERR_PAYLOAD_SIZE:
			LOGI("Publish 数据过大: %s", info);
			break;
		case MOSQ_ERR_MALFORMED_UTF8:
			LOGI("Publish 主题编码是无效的UTF8: %s", info);

			break;
	}

}

char *get_mac(bool CapsLK)
{
	static char mac[64] = {0};

	unsigned int rvalue = 0;

	if(strlen(mac)!=0) {
		return mac;
	}

	if(g_system_cpu_arch == ARCH_MIPS) {
		char mac_cmd[2][128] = {"hexdump -v -n 6 -s 4 -e '6/1 \"%02x\"' /dev/mtd2 2>/dev/null", "hexdump -v -n 6 -s 4 -e '6/1 \"%02X\"' /dev/mtd2 2>/dev/null"};

		memset(mac, 0, sizeof(mac));
		// char mac_cmd[] = "hexdump -v -n 6 -s 4 -e '5/1 \"%02X:\" 1/1 \"%02x\"' /dev/mtd2 2>/dev/null";

		command_output(mac_cmd[CapsLK], mac);
	} else {
				srand((unsigned)time(NULL));

       	rvalue = rand();

		sprintf(mac, "%d", rvalue);

		LOGE("not get mac, rand generate");
	}

	return mac;
}

int msgid_generator(void)
{
	srand((unsigned)time(NULL));

	return rand()%65536;
}

void init_config(struct mosq_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->id = get_mac(false);
	/* cfg->clean_session = true; */
	cfg->port = 1883;
	cfg->host = strdup("localhost");
	cfg->keepalive = 60;
	cfg->mode = true;
	cfg->device = "/dev/ttyS1";
}



extern unsigned char  ble_record_enable;
extern unsigned char  ble_record_log_enable;
extern unsigned char ble_record_filter_rssi;
void config_cmdline_proc(struct mosq_config *cfg, int argc, char **argv)
{
	int c;
	int rc;
	int opterr = 0;

	static struct option long_options[] = {
		{ "device",            required_argument,     NULL, 'e' },
		{ "port",            required_argument,     NULL, 'p' },
		{ "host",            required_argument,     NULL, 'h' },
		{ "keepalive",       required_argument,     NULL, 'k' },
		{ "debug",           no_argument,           NULL, 'd' },
		{ "automaticble",    required_argument,     NULL, 'b' },
		{ "filter",          required_argument,     NULL, 'r' },
		{ "blelog",          required_argument,     NULL, 'g' },
		//{ "disable-clean-session", no_argument,     NULL, 'c' },
		{ "username",        required_argument,     NULL, 'u' },
		{ "password",        required_argument,     NULL, 'P' },
		{ "sub",             required_argument,     NULL, 's' }, 
		{ "pub",             required_argument,     NULL, 'l' },
		//{ "public",          required_argument,     NULL, 'a' },
		{ "mode",            required_argument,     NULL, 'm' },
		{ "id",              required_argument,     NULL, 'i' },
		{ "version",         required_argument,     NULL, 'v' },
		//{ "pid",             required_argument,     NULL, 'f' },
		{ "help",            no_argument,           NULL, GETOPT_VAL_HELP },
		{ 0, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "e:p:h:k:d:b:r:g:u:P:s:i:l:m:v", long_options, NULL)) != -1) {

		switch (c) {
		case GETOPT_VAL_HELP:
			print_usage();
			exit(EXIT_SUCCESS);
			break;
		case 'e':
			cfg->device = strdup(optarg);
			break;
		case 'p':
			cfg->port = atoi(optarg);
			break;
		case 'h':
			cfg->host = strdup(optarg);
			break;
		case 'k':
			cfg->keepalive = atoi(optarg);
			break;
		case 'd':
			cfg->debug = atoi(optarg);
			break;
		case 'c':
			/* cfg->clean_session = false; */
			break;
		case 'u':
			cfg->username = strdup(optarg);
			break;
		case 'P':
			cfg->password = strdup(optarg);
			break;
		case 's':
			cfg->sub = strdup(optarg);
			break;
		case 'l':
			cfg->pub = strdup(optarg);
			break;
//		case 'a':
//			cfg->public = strdup(optarg);
//			break;
		case 'm':
			cfg->mode = atoi(optarg);
			break;
		case 'i':
			cfg->id = strdup(optarg);
			break;
		case 'v':
			cfg->version = atoi(optarg);
			break;
		case 'f':
			cfg->pid_path = strdup(optarg);
			break;
		case 'b':
			ble_record_enable = atoi(optarg);
			break;
		case 'r':
			ble_record_filter_rssi = atoi(optarg);
			break;
		case 'g':
			ble_record_log_enable = atoi(optarg);
			break;
		case '?':
			opterr = 1;
			break;
		}
	}

	exec_command("logger option end");
	if (opterr) {
		print_usage();
		exit(EXIT_FAILURE);
	}
}


int do_reboot(void)
{
	return exec_command("reboot");
}

extern int fd_uuart;

void mqttMessageSend(struct mosquitto *mosq, char *data, int size)
{
	
	TOPIC_NODE_t *poll = g_publish_list;
	while(poll)
	{
		int ret = mosquitto_publish(mosq, NULL, poll->topic, size, data, 0, false);
		mqttReturns(ret, data);
		poll = poll->next;
	}
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	int rc = 0;

	if(DEBUG) {
		LOGI("got message '%.*s' from topic '%s'", message->payloadlen, (char*) message->payload, message->topic);
	}
	

	if(g_mosq_config_t->mode) {
		uint16_t s = writen(fd_uuart, message->payload, message->payloadlen);
		if (s < 0) {
			LOGE("Passthrough: Fail to send message to serial COM\n");
		}
	} else {
		transRecvDownData((char*) message->payload, message->payloadlen, mosq);
	}
}


void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	LOGI("Connection with broker is established");

	if(!result){

		int mid;

		connect_flag = 1;
		
		TOPIC_NODE_t *poll = g_subscribe_list;
		while(poll)
		{
			LOGI("Subscribing single topic: %s", poll->topic);
			mosquitto_subscribe(mosq, &mid, poll->topic, 0);
			poll = poll->next;
		}

		if(!g_mosq_config_t->mode) {
			uint8_t buf_router_info[512];
			memset(buf_router_info, 0, sizeof(buf_router_info));
			transRouterInfo(buf_router_info);

			LOGI("Router info: %s", buf_router_info);

			mqttMessageSend(mosq, buf_router_info, strlen(buf_router_info));

		}
	}
}

void disconnect_callback(struct mosquitto *mosq, void *obj, int result)
{
	LOGI("Connection with broker is unestablished");

	if(!result){

		connect_flag = 0;
		
		LOGI("disconnect_callback");

	}
}


void publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	LOGI("publish callback!");
}

void subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;

	LOGI("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		LOGI(", %d", granted_qos[i]);
	}
	LOGI("\n");
}

void print_usage()
{
	printf("gl_mqtt is a simple mqtt client that will deal with publish and subscribe message in the background.\n");
	printf("Usage: gl_mqtt {[-h host] [-p port] [-u username [-P password]]}\n");
	printf("                     [-k keepalive]\n");
	printf("       gl_mqtt --help\n\n");
	printf(" -e : select uart.\n");
	printf(" -d : enable debug messages.\n");
	printf(" -f : run in the background.\n");
	printf(" -h : mqtt host to connect to. Defaults to localhost.\n");
	printf(" -k : keep alive in seconds for this client. Defaults to 60.\n");
	printf(" -p : network port to connect to. Defaults to 1883 for plain MQTT and 8883 for MQTT over TLS.\n");
	printf(" -u : provide a username\n");
	printf(" -P : provide a password\n");
	printf(" -s : mqtt sub\n");
	printf(" -l : mqtt pub\n");
	printf(" -i : clientId\n");
	printf(" --help : display this message.\n");
}

void file_md5(const char *file, char *md5)
{
	int i = 0;
	uint8_t buf[16] = {0};

	md5sum(file, buf);

	for (i = 0; i < 16; i++) {
		sprintf(md5 + (i * 2), "%02x", (uint8_t) buf[i]);
	}
}

int main(int argc, char **argv)
{
 	setlogmask(~(LOG_ERR | LOG_INFO | LOG_NOTICE | LOG_CRIT | LOG_ALERT | LOG_EMERG));

	static struct mosq_config cfg;
	g_mosq_config_t = &cfg;
	struct mosquitto *mosq = NULL;

	int rc = 0;

	get_cpu_arch_init();

	init_config(&cfg);
	config_cmdline_proc(&cfg, argc, argv);
	DEBUG = cfg.debug;

	protocol_version = cfg.version;

	USE_SYSLOG(argv[0]);

	if (cfg.port < 1 || cfg.port > 65535) {
		LOGE("Invalid port given: %d", cfg.port);
		exit(EXIT_FAILURE);
	}

	if (cfg.keepalive > 65535) {
		LOGE("Invalid keepalive given: %d", cfg.keepalive);
		exit(EXIT_FAILURE);
	}

	uint8_t exe_buf[128];
	if(cfg.pub == NULL) {
		char topic[128] = {0};
		snprintf(topic, sizeof(topic), "sys/cloud/%s", get_mac(false));
		publishTopicAppend(topic);
		memset(exe_buf, 0 ,sizeof(exe_buf));
		sprintf(exe_buf, "uci add_list cositea_mqtt.@server[0].pub=\"%s\"", topic);
		exec_command(exe_buf);
		exec_command("uci commit cositea_mqtt");
	} else {
		char buffer[2048] = {0};
		strcpy(buffer, cfg.pub);
		char *topic=strtok(buffer," ");
		while(topic) {
			publishTopicAppend(topic);
			topic=strtok(NULL," ");
		}
	}
	if(cfg.sub == NULL) {
		char topic[128] = {0};
		snprintf(topic, sizeof(topic), "sys/%s/cloud", get_mac(false));
		subscribeTopicAppend(topic);
		memset(exe_buf, 0 ,sizeof(exe_buf));
		sprintf(exe_buf, "uci add_list cositea_mqtt.@server[0].sub=\"%s\"", topic);
		exec_command(exe_buf);
		exec_command("uci add_list cositea_mqtt.@server[0].sub=\"sys/gateway/cloud\"");
		exec_command("uci commit cositea_mqtt");
	} else {
		char buffer[2048] = {0};
		strcpy(buffer, cfg.sub);
		char *topic=strtok(buffer," ");
		while(topic) {
			subscribeTopicAppend(topic);
			topic=strtok(NULL," ");
		}
	}
	//save topic, will display on web page
//	memset(exe_buf, 0 ,sizeof(exe_buf));
//	sprintf(exe_buf, "echo '%s'>mqtt_pub", g_publish_topic);
//	exec_command(exe_buf);
//	memset(exe_buf, 0 ,sizeof(exe_buf));
//	sprintf(exe_buf, "echo '%s'>mqtt_sub", g_subscribe_topic);
//	exec_command(exe_buf);


	mosquitto_lib_init();

	mosq = mosquitto_new(cfg.id, false, &cfg);
	if (!mosq) {
		switch (errno) {
		case ENOMEM:
			LOGE("Out of memory");
			break;
		case EINVAL:
			LOGE("Invalid id and/or clean_session");
			break;
		}

		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	if (cfg.username && mosquitto_username_pw_set(mosq, cfg.username, cfg.password)) {
		LOGE("Problem setting username and password");

		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	
	/* Create a thread to deal with timer tasks */
	//pthread_t tid = 0;
	cosCreateThread(mosq);
	//pthread_create(&tid, NULL, doit, (void *)mosq);
	//pthread_detach(tid);

	mosquitto_connect_callback_set(mosq, connect_callback);
	/* mosquitto_subscribe_callback_set(mosq, subscribe_callback); */
	mosquitto_message_callback_set(mosq, message_callback);
//	mosquitto_publish_callback_set(mosq, publish_callback);

	mosquitto_disconnect_callback_set(mosq, disconnect_callback);

	LOGI("Connecting to %s:%d", cfg.host, cfg.port);
	mosquitto_connect(mosq, cfg.host, cfg.port, cfg.keepalive);


	rc = mosquitto_loop_forever(mosq, -1, 1);
	if (rc) {
		LOGE("mosq error:%s", mosquitto_strerror(rc));
	}

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	return rc;
}
