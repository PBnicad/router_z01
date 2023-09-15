#ifndef __RDSS_UART_H__#define __RDSS_UART_H__
#include <mosquitto.h>

void rdss_uart_init(void);
void rdssJsonDeal(unsigned char *json_data, unsigned int json_data_len, unsigned char *toBuff, struct mosquitto *mosq);




#endif



