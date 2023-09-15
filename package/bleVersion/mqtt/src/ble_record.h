#ifndef __BLE_RECORD_H__
#define __BLE_RECORD_H__

#include <stdio.h>
#include <stdint.h>

typedef struct {
	unsigned char mac[6];
	unsigned short type;
	unsigned char rssi;
}BLE_INFO_t;

typedef struct ble_record {
	
	BLE_INFO_t device;
	/*如果这个时间超时,则设备已离开基站范围,删除设备*/
	unsigned int timeout;
	/*如果这个时间超时,则需要对设备进行操作*/
	unsigned int send_timeout;
	/*限制不允许再次自动操作的时间*/
	unsigned int busy_timeout;
	/*下一个设备*/
	struct ble_record *next;
}BLE_RECORD_t;


void ble_record(unsigned char *mac, unsigned short type, unsigned char rssi);

void ble_record_list_second_call(void);

void replyConnected(unsigned char *mac, unsigned char status);

void replyPassthrough(unsigned char *mac, unsigned char *data, unsigned int length, unsigned char status);


#endif

