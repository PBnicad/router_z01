#ifndef __COM_H_
#define __COM_H_

#include <libubox/md5.h>
#include <stdbool.h>
#include <mosquitto.h>

#define	MD5_LEN 33

enum{
	ARCH_X86,
	ARCH_ARM,
	ARCH_MIPS,
};

typedef enum {
	MESSAGE_UDP,
	MESSAGE_MQTT,
}MESSAGE_TYPE;

typedef struct {
	char *id;
	int keepalive;
	char *host;
	int port;
	bool debug;      //true: enable log
	char *username;
	char *password;
	char *sub;
	char *pub;
	bool mode;       //true:Passthrough mode
	char *pid_path;
	unsigned char version;
}ACCESS_CONTRL_CONFIG_t;

void mqttReturns(int ret, char *info);

extern ACCESS_CONTRL_CONFIG_t access_contrl_config;
extern struct mosquitto *mosq_config;

extern unsigned char connect_flag;

extern char *get_mac(bool capsCLK);
void build_time_get(char *buf);
void random_generator(char *buf, int len);
void file_md5(const char *file, char *md5);
int msgid_generator(void);
int do_reboot(void);

#endif
