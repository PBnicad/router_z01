#ifndef UDP_NETWORK_H__
#define UDP_NETWORK_H__

#include <stdbool.h>

void udpPthreadCreate(void);
void udpSocketSendMessage(unsigned char *data, unsigned int size);
const unsigned char *udpGetMac(void);
const unsigned char *udpGetIp(void);
const char *updGetAllIpString(void);

#endif //UDP_NETWORK_H__


