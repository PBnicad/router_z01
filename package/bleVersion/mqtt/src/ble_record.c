/*
 * 此文件主要实现功能：
 * (1)记录蓝牙主机上报的蓝牙mac,type
 * (2)根据配置对记录的蓝牙连接发送一些数据指令操作
 *
 *
 * */
#include "ble_record.h"
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

#include "guci.h"
#include "shell.h"
#include <stdarg.h>

unsigned char ble_record_enable = 1;
unsigned char ble_record_log_enable = 1;

unsigned char ble_record_filter_rssi = 55;

/*typedef struct {
	union {
		
	}
}FUNC_ENABLE_t;*/

#define DEVICE_R9  0x04A1
#define DEVICE_X3W 0x0826
#define DEVICE_B10 0x0820

extern int fd_uuart;

/*设备记录超时时间, 操作成功后再次操作的时间*/
const unsigned int ble_record_max_time = 10;
/*定时操作的时间, 可以当作操作失败后再次操作的间隔*/
const unsigned int ble_record_cmd_time = 5;
/*最大同时连接操作的个数*/
#define BLE_CONNECT_MAX        8
/*6bytes mac, 1bytes timeout*/
unsigned char ble_exec_mac[BLE_CONNECT_MAX][7] = {0};

BLE_RECORD_t *ble_list_head = NULL;


void ble_record_log_v(const char *fmt, ...)
{
	if(ble_record_log_enable)
	{
		char log_buff[512] = {0};
		uint16_t len = 0;
		va_list args;
		va_start(args, fmt);
		vsprintf(log_buff, fmt, args);
		va_end(args);

		len = strlen(log_buff);
		exec_command(log_buff);
	}
}

#define ble_record_log(...) ble_record_log_v("logger "__VA_ARGS__);

BLE_RECORD_t *ble_record_malloc(unsigned char *mac, unsigned short type, unsigned char rssi)
{
	if (mac == NULL)
	{
		return NULL;
	}
	if (ble_list_head != NULL)
	{
		BLE_RECORD_t *head = ble_list_head;
		while (head)
		{
			if (memcmp(head->device.mac, mac, sizeof(head->device.mac)) == 0)
			{
				/*找到已存在的设备, 进行覆盖*/
				head->device.rssi = rssi;
				head->device.type = type;
				head->timeout = 0;
				return NULL;
			}
			head = head->next;
		}
	}
	BLE_RECORD_t *record = (BLE_RECORD_t *)malloc(sizeof(BLE_RECORD_t));
	if (record == NULL)
	{
		return NULL;
	}
	memset(record, 0, sizeof(BLE_RECORD_t));
	memcpy(record->device.mac, mac, sizeof(record->device.mac));
	record->device.type = type;
	record->device.rssi = rssi;
	record->timeout = 0;
	/*首次发现,快速操作*/
	record->send_timeout = ble_record_cmd_time-2;
	record->next = NULL;
	return record;
}

BLE_RECORD_t *ble_record_list_tail(void)
{
	if (ble_list_head == NULL)
	{
		return ble_list_head;
	}
	
	BLE_RECORD_t *head = ble_list_head;
	while (head->next)
	{
		head = head->next;
	}
	return head;
}

void ble_record_list_insert(BLE_RECORD_t *record)
{
	if (record == NULL)
	{
		return;
	}
	BLE_RECORD_t *tail = ble_record_list_tail();
	if (tail == NULL)
	{
		ble_list_head = record;
	}
	else
	{
		tail->next = record;
	}
}

/*打印设备*/
void ble_record_list_printf(unsigned short type)
{
	unsigned int number = 0;
	BLE_RECORD_t *head = ble_list_head;
	while (head)
	{
		if(head->device.type==type || type==0)
		{
			ble_record_log("%02x:%02x:%02x:%02x:%02x:%02x %04x %d cnt:%d cmd:%d busy:%d\n", head->device.mac[0], head->device.mac[1],
																			head->device.mac[2], head->device.mac[3],
																			head->device.mac[4], head->device.mac[5], 
																			head->device.type, head->device.rssi, 
																			head->timeout, head->send_timeout, head->busy_timeout);
		}
		number++;
		head = head->next;
	}
	ble_record_log("all device: %d\n", number);
}

/*设置设备繁忙时间,不允许操作*/
void ble_record_set_busy(unsigned char *mac, unsigned int busy_time)
{
	BLE_RECORD_t *head = ble_list_head;
	while (head)
	{
		if(memcmp(head->device.mac, mac, 6)==0)
		{
			head->busy_timeout = busy_time;
			break;
		}
		head = head->next;
	}
}


void ble_record(unsigned char *mac, unsigned short type, unsigned char rssi)
{
	BLE_RECORD_t *record = ble_record_malloc(mac, type, rssi);
	ble_record_list_insert(record);
}

/*判断是否是需要操作的mac,return           -1:不是, other:对应数组序号*/
int ble_mac_cmp(unsigned char *mac)
{
	for(int i=0; i<BLE_CONNECT_MAX; i++)
	{
		if(memcmp(&ble_exec_mac[i][0], mac, 6)==0)
		{
			//ble_record_log("cmpn: %d", i);
			return i;
		}
	}
	return -1;
}


/*查找空闲空间*/
int ble_mac_idle(void)
{
	for(int i=0; i<BLE_CONNECT_MAX; i++)
	{
		if(ble_exec_mac[i][0]==0 && ble_exec_mac[i][1]==0 && ble_exec_mac[i][2]==0 &&
			ble_exec_mac[i][3]==0 && ble_exec_mac[i][4]==0 && ble_exec_mac[i][5]==0)
		{
			return i;
		}
	}
	return -1;
}
/*对超时的操作进行清除*/
void ble_mac_timeout_check(void)
{
	
	for(int i=0; i<BLE_CONNECT_MAX; i++)
	{
		if(ble_exec_mac[i][0]==0 && ble_exec_mac[i][1]==0 && ble_exec_mac[i][2]==0 &&
			ble_exec_mac[i][3]==0 && ble_exec_mac[i][4]==0 && ble_exec_mac[i][5]==0)
		{

		
		}
		else
		{
			ble_exec_mac[i][6]--;
			if(ble_exec_mac[i][6]==0)
			{
				ble_record_log("exec timeout, device: %02x:%02x:%02x:%02x:%02x:%02x", 
					ble_exec_mac[i][0], ble_exec_mac[i][1], ble_exec_mac[i][2], ble_exec_mac[i][3], ble_exec_mac[i][4], ble_exec_mac[i][5]);
				memset(ble_exec_mac[i], 0, sizeof(ble_exec_mac[0]));
			}
		}
	}
}

unsigned short cmdCalCrc(unsigned char *data, unsigned int len)
{
	unsigned short crc = 0;
	for (unsigned int i = 0; i < len; i++) {
		crc += data[i];
	}
	return crc;
}


void cmdConnectDevice(unsigned char *mac)
{
#define SHOER_CONNECT 0x02
#define LONG_CONNECT 0x0B
	const unsigned char dataHead[] = { 0xAA,0x8C,0xCA,0x05,0x5C,0x00,0x01,LONG_CONNECT,0xA0,0x66,0x55,
								0x44,0x33,0x22,0x11,0x06,0x00 };


	unsigned char buf[60] = { 0 };

	memcpy(buf, dataHead, sizeof(dataHead));
	memcpy(&buf[sizeof(dataHead)], mac, 6);

	unsigned short crc = cmdCalCrc(buf, sizeof(dataHead) + 6);

	buf[sizeof(dataHead) + 6] = crc & 0xff;
	buf[sizeof(dataHead) + 7] = crc >> 8 & 0xff;
	buf[sizeof(dataHead) + 8] = 0x55;

	ble_record_log("connect device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	//this->write((char *)buf, sizeof(dataHead) + 9);
	write(fd_uuart, (char *)buf, sizeof(dataHead) + 9);
}



void cmdDisconnectDevice(unsigned char *mac)
{
	unsigned char dataHead[] = { 0xAA,0x8C,0xCA,0x05,0x5C,0x00,0x01,0x03,0xA0,0x66,0x55,
								0x44,0x33,0x22,0x11,0x06,0x00 };

	unsigned char buf[60] = { 0 };

	memcpy(buf, dataHead, sizeof(dataHead));
	memcpy(&buf[sizeof(dataHead)], mac, 6);

	unsigned short crc = cmdCalCrc(buf, sizeof(dataHead) + 6);

	buf[sizeof(dataHead) + 6] = crc & 0xff;
	buf[sizeof(dataHead) + 7] = crc >> 8 & 0xff;
	buf[sizeof(dataHead) + 8] = 0x55;

	//this->write((char *)buf, sizeof(dataHead) + 9);
	ble_record_log("disconnect device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	write(fd_uuart, (char *)buf, sizeof(dataHead) + 9);
}


void cmdQueryConnected(void)
{
	unsigned char data[] = { 0xAA,0x57,0x6D,0xC0,0x5D,0x00,0x01,0x05,0xA0,0xD7,
							0xCC,0xCB,0xCA,0xC9,0xC8,0x00,0x00,0xFA,0x07,0x55 };
	//this->write((char *)data, sizeof(data));
	write(fd_uuart, (char *)data, sizeof(data));

}


void cmdPassthrough(unsigned char *mac, unsigned char *data, unsigned int length)
{

	unsigned char dataHead[] = { 0xAA,0x8C,0xCA,0x05,0x5C,0x00,0x01,0x00,0x20,0x66,0x55,
								0x44,0x33,0x22,0x11,0x07,0x00 };

	unsigned char buf[60] = { 0 };

	memcpy(buf, dataHead, sizeof(dataHead));
	memcpy(&buf[sizeof(dataHead)], mac, 6);
	memcpy(&buf[sizeof(dataHead) + 6], data, length);

	buf[15] = (6 + length) & 0xff;
	buf[16] = (6 + length) >> 8 & 0xff;

	unsigned short crc = cmdCalCrc(buf, sizeof(dataHead) + 6 + length);

	buf[sizeof(dataHead) + 6 + length] = crc & 0xff;
	buf[sizeof(dataHead) + 7 + length] = crc >> 8 & 0xff;
	buf[sizeof(dataHead) + 8 + length] = 0x55;
	ble_record_log("send data to device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	//this->write((char *)buf, sizeof(dataHead) + 9 + length);
	write(fd_uuart, (char *)buf, sizeof(dataHead) + 9 + length);
}


void cmdReboot(void)
{
	unsigned char data[] = { 0xAA,0x8C,0xCA,0x05,0x5C,0x00,0x01,0x0D,0xA0,0xD7,
							0xCC,0xCB,0xCA,0xC9,0xC8,0x00,0x00,0xD8,0x07,0x55 };
	//this->write((char *)data, sizeof(data));
	ble_record_log("reboot");
	write(fd_uuart, (char *)data, sizeof(data));
}
		


void replyConnected(unsigned char *mac, unsigned char status)
{
	if(!ble_record_enable)
		return;
	int ind = 0;
	ble_record_log("connect reply, device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	ind = ble_mac_cmp(mac);
	if(ind==-1)
	{
		return ;
	}
	if(status)
	{
		/*连接失败*/
		//cmdConnectDevice(mac);
		ble_record_log("connect fail, device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		memset(ble_exec_mac[ind], 0, sizeof(ble_exec_mac[0]));
		ble_record_set_busy(mac, 10);
	}
	else
	{
		ble_record_log("connect success, send cmd. device: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		cmdPassthrough(mac, "1", 1);
	}
}


void replyPassthrough(unsigned char *mac, unsigned char *data, unsigned int length, unsigned char status)
{
	if(!ble_record_enable)
		return;
	int ind = 0;
	ind = ble_mac_cmp(mac);
	if(ind==-1)
	{
		return ;
	}
	//
	ble_record_log("reply passthrough %d, %02x:%02x:%02x:%02x:%02x:%02x", length, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	for(int i = 0; i<length; i++)
	{
		ble_record_log("%02x", data[i]);
	}
	if(data[0]=='2' && length==1)
	{
		/*操作完成,限制下次操作时间*/
		/*设置成功后1分钟内不允许操作*/
		ble_record_set_busy(mac, 60);
	}
	/*接收到响应后不管是否正确,执行断开*/
	memset(ble_exec_mac[ind], 0, sizeof(ble_exec_mac[0]));
	cmdDisconnectDevice(mac);
}


void ble_record_list_second_call(void)
{
	if(!ble_record_enable)
		return;
	BLE_RECORD_t *head = ble_list_head;
	static unsigned int ble_start_exec = 0;
	ble_mac_timeout_check();
	
	static uint16_t reboot_ticks = 0;
	if(reboot_ticks++>=180) {
		cmdReboot();
		reboot_ticks = 0;
	}
	
	while (head)
	{
		head->send_timeout = head->send_timeout+1;
		if(head->send_timeout>=ble_record_cmd_time && head->busy_timeout==0)
		{
			if(head->device.type == 0x04A1 && head->device.rssi<ble_record_filter_rssi)
			{
				//记录设备对设备进行操作
				
				int ind = 0;
				ind = ble_mac_cmp(head->device.mac);
				if(ind==-1)
				{
					/*查询有没有空闲操作空间*/
					ind = ble_mac_idle();
					if(ind!=-1)
					{
						memset(ble_exec_mac[ind], 0, sizeof(ble_exec_mac[0]));
						memcpy(ble_exec_mac[ind], head->device.mac, 6);
						ble_exec_mac[ind][6] = 30; //设置15秒超时时间,超时后删除操作
						cmdConnectDevice(head->device.mac);
						head->send_timeout = 0;
					}
				}
				else
				{
					/*已有该设备的操作存在*/
				}
			}
			else
			{
				head->send_timeout = 0;
			}
		}
		
		if(head->busy_timeout)
		{
			head->busy_timeout--;
		}
		
		head->timeout = head->timeout + 1;
		if (head->timeout >= ble_record_max_time)
		{
			//超时,删除设备
			BLE_RECORD_t *free_device = head;      //记录超时设备
			BLE_RECORD_t *last = ble_list_head;    //
			BLE_RECORD_t *query = ble_list_head;   //从头开始查询
			while (query != free_device && query)  //遍历找到超时设备
			{
				last = query;                      //记录上一个设备
				query = query->next;               //
			}
			if (query == ble_list_head)
			{
				ble_list_head = query->next;
			}
			else
			{
				last->next = query->next;
			}
			head = head->next;
			free(query);
		}
		else
		{
			head = head->next;
		}
	}

	static unsigned char printf_cnt = 0;
	if(printf_cnt++>=10)
	{
		printf_cnt = 0;
		ble_record_list_printf(DEVICE_R9);
	}
}



