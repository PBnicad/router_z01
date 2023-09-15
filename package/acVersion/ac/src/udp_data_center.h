#ifndef UDP_DATA_CENTER_H__
#define UDP_DATA_CENTER_H__

#include <stdbool.h>

void udpMessageHandler(unsigned char *data, unsigned int len);
unsigned short crc16_compute(unsigned char const * p_data, unsigned int size, unsigned short const * p_crc);
void udpMessageHeartbeat(void);

/* 遍历共享信息使用, 类似strtok的用法, 第一次调用first传true, 
    后续一直调用实现遍历直至返回false
	参考:
	unsigned char mac[6] = {0};
	char *ipstr = NULL;
	bool loop=gwShareInfoViewTok(true, mac, ip);
	while(loop) {
		printf("view info %s\n", ip);
		memset(mac, 0, sizeof(mac));
		loop = gwShareInfoViewTok(false, mac, ip);
	}
*/
bool gwShareInfoViewTok(bool first, unsigned char *mac, char **ipStr);


#endif //UDP_DATA_CENTER_H__

