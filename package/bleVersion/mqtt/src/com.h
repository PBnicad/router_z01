#ifndef __COM_H_
#define __COM_H_

#include <mosquitto.h>

#define ARCH_X86  1
#define ARCH_ARM  2
#define ARCH_MIPS 3


struct mosq_config {
	char *id;
	int keepalive;
	char *host;
	int port;
	bool debug;
	char *username;
	char *password;
	char *sub;
	char *pub;
	char *public;
	bool mode;
	char *pid_path;
	char *device;
	unsigned char version;
};
void mqttReturns(int ret, char *info);

extern unsigned char connect_flag;

extern char *get_mac(bool capsCLK);
void mqttMessageSend(struct mosquitto *mosq, char *data, int size);


#endif
