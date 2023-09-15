/************************************************************************************* 
filename:     	transcoder.c
create:       	2019.11.20 LUJUNLIN
revised:     	
revision:     	1.0
description:	Convert hexadecimal data transfer to json data transfer
**************************************************************************************/

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

#include "transcoder.h"
#include "utils.h"
#include "gjson.h"
#include "packet.h"
#include "guci.h"
#include "shell.h"
#include "ble_record.h"

bool DEBUG = false;



//function code
enum {
	CODE_PASSTHROUGH				= 0x00,
	CODE_TIME_UPDATE				= 0x01,
	CODE_CONNECT_DEVICE_SHORT		= 0x02,
	CODE_DISCONNECT_DEVICE			= 0x03,
	CODE_QUERY_DEVICE_STATUS		= 0x04,
	CODE_QUERY_CONNECTED_NUM		= 0x05,
	CODE_START_SCAN					= 0x06,
	CODE_STOP_SCAN					= 0x07,
	CODE_QUERY_GATEWAY_FIRMWARE		= 0x08,
	CODE_GATEWAY_FIRMWARE_UPDATE	= 0x09,
//	CODE_DEVICE_FIRMWARE_UPDATE		= 0x0A,
	CODE_CONNECT_DEVICE_LONG		= 0x0B,
//	CODE_TEPORT_MATTER				= 0x0C,
	CODE_REBOOT						= 0x0D,
	CODE_FACTORY_RESET				= 0x0E,
	CODE_SCAN_ARG_CONFIG			= 0x13,
	CODE_SCAN_ARG_QUERY				= 0x14,
	CODE_QUERY_FIRMWARE             = 0x15,
	CODE_WARING                     = 0x16,
	CODE_ONLINE						= 0x40,		// 
	CODE_REPORT_BORDCAT				= 0x41,
	CODE_REPORT_BORDCAT_ALL			= 0x42,
	CODE_HEARTBEAT					= 0x43,
}FUNCTION_CODE;

//object string
#define	TRANS_JOBJ_COMTYPE	"comType"
#define	TRANS_JOBJ_CONTENT	"content"
#define	TRANS_JOBJ_DATA		"data"

//key string
#define	TRANS_STRING_PASSTHROUGH		"Passthrough"		//0x00
#define	TRANS_STRING_TIMEUPDATE			"TimeUpdate"		//0x01
#define	TRANS_STRING_CONNECTDEVICE		"ConnectDevice"
#define	TRANS_STRING_DISCONNECTDEVICE	"DisconnectDevice"
#define	TRANS_STRING_DEVICESTATUS		"DeviceStatus"
#define TRANS_STRING_QUERYCONECTED		"QueryConnected"
#define	TRANS_STRING_STARTSCAN			"StartScan"
#define	TRANS_STRING_STOPSACN			"StopScan"
#define	TRANS_STRING_FIRMWAREVERSION	"FirmwareVersion"
#define	TRANS_STRING_FIRMWAREUPDATE		"FirmwareUpdate"
#define	TRANS_STRING_CONNECTDEVICELONG	"ConnectDeviceLong"
#define	TRANS_STRING_REPORTMATTER		"ReportMatter"
#define	TRANS_STRING_REBOOT				"Reboot"
#define	TRANS_STRING_FACTORYTESET		"FactoryTeset"
#define	TRANS_STRING_ONLINE				"Online"
#define	TRANS_STRING_REPORTBORDCAST		"ReportBordcast"
#define	TRANS_STRING_REPORTBORDCASTALL	"ReportBordcastAll"
#define	TRANS_STRING_REPORTBORDCASTOTHER	"ReportBordcastOther"
#define	TRANS_STRING_SYSUPGRADE			"Upgrade"
#define	TRANS_STRING_HEARTBEAT			"Heartbeat"
#define	TRANS_STRING_SCANCONFIG			"ScanConfig"
#define	TRANS_STRING_SACNQUERY			"ScanQuery"
#define TRANS_STRING_QUERYFIRMWARE      "QueryFirmware"
#define TRANS_STRING_WARING             "Waring"

//header tail
static const uint8_t uc_pack_header = 0xAA;
static const uint8_t uc_pack_tail = 0x55;

//临时修复变量定义
static uint8_t frame_functype_buff = CODE_ONLINE;

extern uint8_t protocol_version;
extern int fd_uuart;


//one json pack
typedef struct {
	json_object *jobj;
	json_object *jobj_comType;
	json_object *jobj_content;
	json_object *jobj_data;
}TRANS_JOBJ_t;

static uint8_t transJsonPassthrough(json_object *jobj, uint8_t *data);
static void transJsonBordcast(json_object *jobj, uint8_t *bordcast_data, bool filter);
static void transJsonReportMatter(json_object *jobj, uint8_t *report_data);
static void transJsonScanConfig(json_object *jobj, uint8_t *report_data);
static void transJsonScanQuery(json_object *jobj, uint8_t *report_data);
static void transJsonQueryFirmware(json_object *jobj, uint8_t *report_data);
static void transJsonConnectDeviceLong(json_object *jobj, uint8_t *Connect_data);
static void transJsonQueryGatewayVersion(json_object *jobj, uint8_t *version_data);
static void transJsonStopScan(json_object *jobj, uint8_t *stop_data);
static void transJsonStartScan(json_object *jobj, uint8_t *start_data);
static void transJsonQueryConnected(json_object *jobj, uint8_t *connected_data);
static void transJsonQueryDeviceStatus(json_object *jobj, uint8_t *status_data);
static void transJsonDisconnectDevice(json_object *jobj, uint8_t *data);
static void transJsonConnectDevice(json_object *jobj, uint8_t *data);
static void transJsonUpdateTime(json_object *jobj, uint8_t *data);
static void transJsonFirmwareUpdate(json_object *jobj, uint8_t *data);
static void transJsonWaring(json_object *jobj, uint8_t *report_data);
static void transJsonException(json_object *jobj, uint8_t *data);

static uint16_t transHexReboot(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexReportMatter(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexFactoryTeset(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexTimeUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexPassthrough(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexScanConfig(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexScanQuery(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexQueryFirmware(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexWaring(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexConnectDeviceLong(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexFirmwareVersion(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexFirmwareUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexStopScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexStartScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexQueryConnected(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexDeviceStatus(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexDisconnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff);
static uint16_t transHexConnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff);


/*****************************************************************************************************
@function:      void transDebug(uint8_t *data, uint16_t len)
@description:  	Outputs hexadecimal data to the log
@param:			data	:hex data
				len		:hex data len
@return:        none
******************************************************************************************************/
void transDebug(uint8_t *data, uint16_t len)
{
	if(!DEBUG)
		return ;
	uint8_t execmd[1024] = {0};
	uint8_t buf[500*3+6] = {0};
	uint16_t i=0, index=0;
//	LOGI("**********************len:%d**********************\n", len);
	while(len)
	{
		sprintf(&buf[i],"%02X ", data[index]);
		i += 3;
		if(i>=180) {
			sprintf(&buf[i], "index:%d", index);
			LOGI("%s", buf);
			
			//sprintf(execmd, "logger %s", buf);
			//exec_command(execmd);
			i=0;
		}
		len--;
		index++;
	}
	if(i) {
		LOGI("%s", buf);
		sprintf(execmd, "logger %s", buf);
		exec_command(execmd);
	}
}
void transHexDebug(uint8_t *data, uint16_t len)
{
	uint8_t execmd[1024] = {0};
	uint8_t buf[500*3+6] = {0};
	uint16_t i=0, index=0;
//	LOGI("**********************len:%d**********************\n", len);
	while(len)
	{
		sprintf(&buf[i],"%02X ", data[index]);
		i += 3;
		if(i>=180) {
			sprintf(&buf[i], "index:%d", index);
			LOGI("%s", buf);
			
			sprintf(execmd, "logger %s", buf);
			exec_command(execmd);
			i=0;
		}
		len--;
		index++;
	}
	if(i) {
		LOGI("%s", buf);
		sprintf(execmd, "logger %s", buf);
		exec_command(execmd);
	}
}

/*****************************************************************************************************
@function:      static void transHexToAscii(uint8_t *ascii, uint8_t *hex, uint16_t len)
@description:  	Hexadecimal data into ASCII data
@param:			ascii		:output ascii data
				hex			:hex data
				len			:hex data len;
@return:        1 succes, 0 fail
******************************************************************************************************/
static uint8_t transHexToAscii(uint8_t *ascii, uint8_t *hex, uint16_t len)
{
	if(hex==NULL || ascii==NULL){
		return 0;
	}
	for(int i=0;i<len;i++)
	{
		sprintf(&ascii[2*i], "%02X", hex[i]);
	}
	return 1;
}


/*****************************************************************************************************
@function:      static uint16_t transAsiicToHex(uint8_t *asiic, uint8_t *hex, int len)
@description:  	ASCII data into Hexadecimal data
@param:			ascii	:ascii data
				hex		:output hex data
				len		:ascii data len
@return:        1 succes, 0 fail
******************************************************************************************************/
static uint8_t transAsiicToHex(uint8_t *asiic, uint8_t *hex, int len)
{
	if(hex==NULL || asiic==NULL){
		return 0;
	}
	for(int i=0;i<len/2;i++) {
		if(sscanf((char *)&asiic[i*2], "%02hhX", &hex[i])==-1)
			return 0;
	}
	return 1;
}


/*****************************************************************************************************
@function:      void  transCenterGetLanMac(uint8_t *ver)
@description:   Gets the physical address of the br-lan
@param:			ver	:output mac
@return:        none
******************************************************************************************************/
static void  transCenterGetLanMac(uint8_t *ver)
{
	FILE *verPtr;
	uint8_t lineStr[256];
	strcpy(ver,"00:00:00:00:00:00");
	verPtr = popen("cat /sys/class/net/br-lan/address","r");
	if(verPtr != NULL)    {
		//read a line info
		//if(fgets(lineStr,256,verPtr) != NULL){
		if(fread(lineStr, sizeof(uint8_t), 256, verPtr) > 0){
			strcpy(ver,lineStr);        
		}
		pclose(verPtr);    
	}
}


/*****************************************************************************************************
@function:      static void transHeartBeat(uint8_t * toBuff,  char *msgId)
@description:  	HeartBeat
@param:			toBuff		: output reply heartbeat data
				msgId		:message id
@return:        none
******************************************************************************************************/
static void transHeartBeat(uint8_t * toBuff,  char *msgId)
{
	uint8_t buf[64] = {0};
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();


	json_object_object_add(jobj_t.jobj_comType, "type", json_object_new_string("Up"));

	strcpy(buf, msgId);
	json_object_object_add(jobj_t.jobj_comType, "msgId", json_object_new_string(buf));

	uint8_t clientId[24];
	memset(clientId, 0, sizeof(clientId));
	
	memcpy(clientId, get_mac(true), 12);
	srand((unsigned)time(NULL));
	clientId[12] = rand()%10 + '0';
	clientId[13] = rand()%10 + '0';
	clientId[14] = rand()%10 + '0';
	clientId[15] = '\0';

	json_object_object_add(jobj_t.jobj_comType, "clientId", json_object_new_string(clientId));

	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string("Heartbeat"));
	
	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);

	memcpy(toBuff, json_object_to_json_string(jobj_t.jobj), strlen(json_object_to_json_string(jobj_t.jobj)));
	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}


/*****************************************************************************************************
@function:      void transJsonHearder(TRANS_JOBJ_t *jobj_t)
@description:   conversion "clientId", "message id" to json object data
@param:         jobj_t: json object pack
@return:        none
******************************************************************************************************/
void transJsonHearder(TRANS_JOBJ_t *jobj_t)
{
	char buf[64] = {0};

	json_object_object_add(jobj_t->jobj_comType, "type", json_object_new_string("Up"));

	uint32_t utc_time = time(NULL);
	sprintf(buf, "%d", utc_time);
	json_object_object_add(jobj_t->jobj_comType, "msgId", json_object_new_string(buf));

	uint8_t clientId[24];
	memset(clientId, 0, sizeof(clientId));
	
	memcpy(clientId, get_mac(true), 12);

	json_object_object_add(jobj_t->jobj_comType, "clientId", json_object_new_string(clientId));
}


/*****************************************************************************************************
@function:      static void transJsonBordcast(json_object *jobj, uint8_t *bordcast_data)
@description:  	convert Bordcast hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
			 	filter		:[true] not filter; [false] filter
@return:        none
******************************************************************************************************/
static void transJsonBordcast(json_object *jobj, uint8_t *bordcast_data, bool filter)
{
	transDebug(bordcast_data, 4);
	//frame length, 帧长度
	uint16_t data_len = *(uint16_t *)bordcast_data;
	
	//size = 1bytes(len) + 2bytes(type) + 6bytes(mac) + 1bytes(rssi) + nbytes(B8)	0x41
	//size = 1bytes(len) + 6bytes(mac) + 1bytes(rssi) + nbytes()					0x42
	uint8_t pack_head_size = 8;
	if(filter) {
		pack_head_size = 10;
	}
	if(DEBUG) {
		LOGI("size %d \n", data_len);
	}
	
	if(data_len==1)
	{
		json_object_object_add(jobj, "number", json_object_new_string("0"));
		return ;
	}
	
	bordcast_data += 2;

	//device number
	uint16_t dev_num = *(uint16_t *)bordcast_data;
	if(DEBUG) {
		LOGI("Device %d \n", dev_num);
	}
	uint8_t buf_num[12] = {0};
	sprintf(buf_num, "%d", dev_num);
	json_object_object_add(jobj, "number", json_object_new_string(buf_num));

	int i = 1;
	char key_buf[32];
	char buf[200] = {0};
	uint16_t dev_pack_size = 0;
	bordcast_data += 2;
	//dev_num = 3;
	unsigned short type = 0;
	while (dev_num)
	{
		//offset
		bordcast_data += dev_pack_size;

		/*one packect size, bordcast_data[0] is b8 data size*/
		dev_pack_size = bordcast_data[0] + pack_head_size;

		if(filter) {
			//type
			sprintf(key_buf, "type%d", i);
			sprintf(buf, "%02X%02X", bordcast_data[pack_head_size-8], bordcast_data[pack_head_size-9]);
			json_object_object_add(jobj, key_buf, json_object_new_string(buf));
			type = bordcast_data[pack_head_size-8]<<8 | bordcast_data[pack_head_size-9];
		}
		else
		{
			/*
			len mac rssi
			020106
			03030D18
			16FFB80000000040FF0000000002EA60A26041641C1662
			0BFFB60820B7DD3F2EDB1381
			11094231305F444433463245444231333831
			*/
			unsigned int ind_find_b6 = pack_head_size;
			//transHexDebug(&bordcast_data[ind_find_b6], pack_head_size);

			while(ind_find_b6 < dev_pack_size)
			{
				//transHexDebug(&bordcast_data[ind_find_b6], bordcast_data[ind_find_b6]+1);
				if(bordcast_data[ind_find_b6+1]==0xff && bordcast_data[ind_find_b6+2]==0xb6)
				{
					type = bordcast_data[ind_find_b6+3]<<8 | bordcast_data[ind_find_b6+4];
					//char dd[50] = {0};
					//sprintf("logger type:%04x, %02x, %02x", type, bordcast_data[ind_find_b6+3], bordcast_data[ind_find_b6+4]);
					break;
				}
				else
				{
					ind_find_b6 += bordcast_data[ind_find_b6]+1;
					if(bordcast_data[ind_find_b6]==0)
					{
						exec_command("logger length err");
						break;
					}
				}
			}
		}

		//mac
		sprintf(key_buf, "mac%d", i);
		sprintf(buf, "%02X%02X%02X%02X%02X%02X", bordcast_data[pack_head_size-2], bordcast_data[pack_head_size-3], \
			bordcast_data[pack_head_size-4], bordcast_data[pack_head_size-5], bordcast_data[pack_head_size-6], bordcast_data[pack_head_size-7]);
		json_object_object_add(jobj, key_buf, json_object_new_string(buf));
		if(DEBUG) {
			uint8_t buff[128];
			sprintf(buff, "logger %s", buf);
			exec_command(buff);
		}

		//rssi
		sprintf(key_buf, "rssi%d", i);
		sprintf(buf, "%02X", bordcast_data[pack_head_size-1]);
		json_object_object_add(jobj, key_buf, json_object_new_string(buf));

		//B8
		sprintf(key_buf, "pack%d", i);
		transHexToAscii(buf, &bordcast_data[pack_head_size], bordcast_data[0]);
		json_object_object_add(jobj, key_buf, json_object_new_string(buf));

		if(DEBUG) {
			uint8_t buff[256];
			sprintf(buff, "logger %s", buf);
			exec_command(buff);
		}

		ble_record(&bordcast_data[pack_head_size-7], type, bordcast_data[pack_head_size-1]);
		
		i++;
		dev_num--;
	}
}

#define REPORT_MATTER_OPEN		0
#define	REPORT_MATTER_CLOSE		1
#define REPORT_MATTER_SETTIME	2
#define REPORT_MATTER_STATUS	3
#define REPORT_MATTER_GETTIME	4

#define REPORT_RESPPONSE_OK		0
#define REPORT_RESPPONSE_FAIL	1
#define REPORT_RESPPONSE_BUSY	2

#define REPORT_STATUS_OPEN		1
#define	REPORT_STATUS_CLOSE		0


/*****************************************************************************************************
@function:      static void transJsonReportMatter(json_object *jobj, uint8_t *report_data)
@description:  	convert ReportMatter hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonReportMatter(json_object *jobj, uint8_t *report_data)
{
	uint8_t buf[8];
	memset(buf, 0, sizeof(buf));
	switch(report_data[2])
	{
		case REPORT_MATTER_OPEN:
		case REPORT_MATTER_CLOSE:
		case REPORT_MATTER_SETTIME:
			if(report_data[3]==REPORT_RESPPONSE_OK) {
				json_object_object_add(jobj, "Response", json_object_new_string("Ok"));
			}else if(report_data[3]==REPORT_RESPPONSE_FAIL) {
				json_object_object_add(jobj, "Response", json_object_new_string("Fail"));
			}else if(report_data[3]==REPORT_RESPPONSE_BUSY) {
				json_object_object_add(jobj, "Response", json_object_new_string("Busy"));
			}
			break;
		case REPORT_MATTER_STATUS:
			if(report_data[3]==REPORT_STATUS_CLOSE) {
				json_object_object_add(jobj, "Response", json_object_new_string("Close"));
			}else if(report_data[3]==REPORT_STATUS_OPEN) {
				json_object_object_add(jobj, "Response", json_object_new_string("Open"));
			}
			break;
		case REPORT_MATTER_GETTIME:
			sprintf(buf, "%d", report_data[3]);
			json_object_object_add(jobj, "Response", json_object_new_string(buf));
			break;
		default:
			break;
	}
}

/*****************************************************************************************************
@function:      static void transJsonReportMatter(json_object *jobj, uint8_t *report_data)
@description:  	convert ReportMatter hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonScanConfig(json_object *jobj, uint8_t *report_data)
{
	uint8_t buf[8];
	memset(buf, 0, sizeof(buf));

	if(report_data[2]==0x00) {
		json_object_object_add(jobj, "response", json_object_new_string("Ok"));
	}else if(report_data[2]==0x01) {
		json_object_object_add(jobj, "response", json_object_new_string("Fail"));
	}

	
	if(report_data[3]==0x00) {
		json_object_object_add(jobj, "switch", json_object_new_string("Close"));
	}else if(report_data[3]==0x01) {
		json_object_object_add(jobj, "switch", json_object_new_string("Open"));
	}

	if(report_data[4]==0x00) {
		json_object_object_add(jobj, "mode", json_object_new_string("Single"));
	}else if(report_data[4]==0x01) {
		json_object_object_add(jobj, "mode", json_object_new_string("Keep"));
	}

	sprintf(buf, "%d", report_data[5]);
	json_object_object_add(jobj, "cycle", json_object_new_string(buf));
			
	if(report_data[6]==0x00) {
		json_object_object_add(jobj, "filter", json_object_new_string("True"));
		frame_functype_buff = 0x41;
	}else if(report_data[6]==0x01) {
		json_object_object_add(jobj, "filter", json_object_new_string("False"));
		frame_functype_buff = 0x42;
	}else if(report_data[6]==0x02) {
		json_object_object_add(jobj, "filter", json_object_new_string("Other"));
		frame_functype_buff = 0x43;
	}
}

static void transJsonScanQuery(json_object *jobj, uint8_t *report_data)
{
	uint8_t buf[8];
	memset(buf, 0, sizeof(buf));
	
	if(report_data[2]==0x00) {
		json_object_object_add(jobj, "switch", json_object_new_string("Close"));
	}else if(report_data[2]==0x01) {
		json_object_object_add(jobj, "switch", json_object_new_string("Open"));
	}

	if(report_data[3]==0x00) {
		json_object_object_add(jobj, "mode", json_object_new_string("Single"));
	}else if(report_data[3]==0x01) {
		json_object_object_add(jobj, "mode", json_object_new_string("Keep"));
	}

	sprintf(buf, "%d", report_data[4]);
	json_object_object_add(jobj, "cycle", json_object_new_string(buf));
			
	if(report_data[5]==0x00) {
		json_object_object_add(jobj, "filter", json_object_new_string("True"));
		frame_functype_buff = 0x41;
	}else if(report_data[5]==0x01) {
		json_object_object_add(jobj, "filter", json_object_new_string("False"));
		frame_functype_buff = 0x42;
	}else if(report_data[6]==0x02) {
		json_object_object_add(jobj, "filter", json_object_new_string("Other"));
		frame_functype_buff = 0x43;
	}
}

static void transJsonQueryFirmware(json_object *jobj, uint8_t *report_data)
{
	uint8_t buf[120] = {0};
	
	uint16_t len = report_data[0] | report_data[1] << 8;
	memcpy(buf, &report_data[2], len);
	
	json_object_object_add(jobj, "version", json_object_new_string(buf));
	
	char cmd[256] = {0};
	snprintf(cmd, sizeof(cmd), "echo '%s'>/ble_version", buf);
	exec_command(cmd);
}

static void transJsonWaring(json_object *jobj, uint8_t *report_data)
{
	char waring_lvl[10] = {0};
	
	snprintf(waring_lvl, sizeof(waring_lvl), "%d", report_data[2]);

	json_object_object_add(jobj, "waringType", json_object_new_string(waring_lvl));
}

#define	CONNECT_DEVICE_OK			0x00
#define	CONNECT_DEVICE_TIMEOUT		0x01
#define	CONNECT_DEVICE_REQUEST_FAIL	0x02
#define	CONNECT_DEVICE_MAX			0x03
#define CONNECT_DEVICE_REPEAT		0x04


/*****************************************************************************************************
@function:      static void transJsonConnectDeviceLong(json_object *jobj, uint8_t *Connect_data)
@description:  	convert ConnectDeviceLong hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonConnectDeviceLong(json_object *jobj, uint8_t *Connect_data)
{
	replyConnected(&Connect_data[3], Connect_data[2]);
	switch(Connect_data[2])
	{
		case CONNECT_DEVICE_OK:
			json_object_object_add(jobj, "response", json_object_new_string("Ok"));
			break;
		case CONNECT_DEVICE_TIMEOUT:
			json_object_object_add(jobj, "response", json_object_new_string("Timeout"));
			break;
		case CONNECT_DEVICE_REQUEST_FAIL:
			json_object_object_add(jobj, "response", json_object_new_string("Fail"));
			break;
		case CONNECT_DEVICE_MAX:
			json_object_object_add(jobj, "response", json_object_new_string("Max"));
			break;
		case CONNECT_DEVICE_REPEAT:
			json_object_object_add(jobj, "response", json_object_new_string("Repeat"));
			break;
		default:
			break;
	}
	char mac[16] = {0};
	sprintf(mac,"%02X%02X%02X%02X%02X%02X",Connect_data[8],Connect_data[7],Connect_data[6],Connect_data[5],Connect_data[4],Connect_data[3]);
	json_object_object_add(jobj, "mac", json_object_new_string(mac));
}


/*****************************************************************************************************
@function:      static void transJsonQueryGatewayVersion(json_object *jobj, uint8_t *version_data)
@description:  	convert QueryGatewayVersion hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonQueryGatewayVersion(json_object *jobj, uint8_t *version_data)
{
	uint8_t app[12] = {0};
	uint8_t bootloader[12] = {0};
	uint8_t softdevice[12] = {0};
	sprintf(app, "%d", *(uint16_t *)&version_data[3]);
	sprintf(bootloader, "%d", *(uint16_t *)&version_data[6]);
	sprintf(softdevice, "%d", *(uint16_t *)&version_data[9]);
	json_object_object_add(jobj, "app", json_object_new_string(app));
	json_object_object_add(jobj, "bootloader", json_object_new_string(bootloader));
	json_object_object_add(jobj, "softdevice", json_object_new_string(softdevice));
}


/*****************************************************************************************************
@function:      static void transJsonStopScan(json_object *jobj, uint8_t *stop_data)
@description:  	convert Stop Scan hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonStopScan(json_object *jobj, uint8_t *stop_data)
{
	switch(stop_data[2])
	{
		case 0:
			json_object_object_add(jobj, "response", json_object_new_string("Ok"));
			break;
		case 1:
			json_object_object_add(jobj, "response", json_object_new_string("Fail"));
			break;
		default:
			json_object_object_add(jobj, "response", json_object_new_string("Error"));
			break;
	}
}


/*****************************************************************************************************
@function:      static void transJsonStartScan(json_object *jobj, uint8_t *start_data)
@description:  	convert Start Scan hexadecimal data to json data
@param:			jobj		:output josn object
				start_data	:data
@return:        none
******************************************************************************************************/
static void transJsonStartScan(json_object *jobj, uint8_t *start_data)
{
	switch(start_data[2])
	{
		case 0:
			json_object_object_add(jobj, "response", json_object_new_string("Ok"));
			break;
		case 1:
			json_object_object_add(jobj, "response", json_object_new_string("Fail"));
			break;
		default:
			json_object_object_add(jobj, "response", json_object_new_string("Error"));
			break;
	}
}


/*****************************************************************************************************
@function:      static void transJsonQueryConnected(json_object *jobj, uint8_t *connected_data)
@description:  	convert Query connected hexadecimal data to json data
@param:			jobj			:output josn object
				connected_data	:data
@return:        none
******************************************************************************************************/
static void transJsonQueryConnected(json_object *jobj, uint8_t *connected_data)
{
	uint8_t connected_num = connected_data[2];
	uint8_t buf_num[8];
	sprintf(buf_num, "%d", connected_num);
	json_object_object_add(jobj, "number", json_object_new_string(buf_num));

	uint8_t key_buf[16] = {0};
	uint8_t mac_buf[32] = {0};
	for(uint8_t i=0; i<connected_num; i++ )
	{
		sprintf(key_buf, "mac%d", i);
		sprintf(mac_buf,"%02X%02X%02X%02X%02X%02X",connected_data[8+i*6],connected_data[7+i*6],\
			connected_data[6+i*6],connected_data[5+i*6],connected_data[4+i*6],connected_data[3+i*6]);

		json_object_object_add(jobj, key_buf, json_object_new_string(mac_buf));
	}
}


/*****************************************************************************************************
@function:      static void transJsonQueryDeviceStatus(json_object *jobj, uint8_t *status_data)
@description:  	convert Query Device Status hexadecimal data to json data
@param:			jobj		:output josn object
				status_data	:data
@return:        none
******************************************************************************************************/
static void transJsonQueryDeviceStatus(json_object *jobj, uint8_t *status_data)
{
	uint8_t mac_buf[32] = {0};
	sprintf(mac_buf,"%02X%02X%02X%02X%02X%02X", status_data[8], status_data[7], \
		status_data[6], status_data[5], status_data[4], status_data[3]);
	json_object_object_add(jobj, "mac", json_object_new_string(mac_buf));
	if(status_data[2]==0) {
		json_object_object_add(jobj, "status", json_object_new_string("Connect"));
	}else if(status_data[2]==1){
		json_object_object_add(jobj, "status", json_object_new_string("Disconnect"));
	}else {
		json_object_object_add(jobj, "status", json_object_new_string("Error"));
	}
}


/*****************************************************************************************************
@function:      static void transJsonDisconnectDevice(json_object *jobj, uint8_t *data)
@description:  	convert DisconnectDevice hexadecimal data to json data
@param:			jobj	:output josn object
				data	:data
@return:        none
******************************************************************************************************/
static void transJsonDisconnectDevice(json_object *jobj, uint8_t *data)
{
	uint8_t mac_buf[32] = {0};
	sprintf(mac_buf,"%02X%02X%02X%02X%02X%02X", data[8], data[7], \
			data[6], data[5], data[4], data[3]);
	json_object_object_add(jobj, "mac", json_object_new_string(mac_buf));
	if(data[2]==0) {
		json_object_object_add(jobj, "response", json_object_new_string("Ok"));
	}else if(data[2]==1){
		json_object_object_add(jobj, "response", json_object_new_string("Fail"));
	}else {
		json_object_object_add(jobj, "response", json_object_new_string("Error"));
	}
}


/*****************************************************************************************************
@function:      static void transJsonConnectDevice(json_object *jobj, uint8_t *data)
@description:  	convert ConnectDevice hexadecimal data to json data
@param:			jobj	:output josn object
				data	:data
@return:        none
******************************************************************************************************/
static void transJsonConnectDevice(json_object *jobj, uint8_t *data)
{
	uint8_t mac_buf[32] = {0};
	sprintf(mac_buf,"%02X%02X%02X%02X%02X%02X", data[8], data[7], \
			data[6], data[5], data[4], data[3]);
	json_object_object_add(jobj, "mac", json_object_new_string(mac_buf));
	
	replyConnected(&data[3], data[2]);
	switch(data[2])
	{
		case 0:
			json_object_object_add(jobj, "response", json_object_new_string("Ok"));
			break;
		case 1:
			json_object_object_add(jobj, "response", json_object_new_string("NotFound"));
			break;
		case 2:
			json_object_object_add(jobj, "response", json_object_new_string("Fail"));
			break;
		case 3:
			json_object_object_add(jobj, "response", json_object_new_string("Max"));
			break;
		case 4:
			json_object_object_add(jobj, "response", json_object_new_string("Repeat"));
			break;
		default:
			json_object_object_add(jobj, "response", json_object_new_string("Error"));
			break;
	}
}


enum DATA_BUFFER_STATUS{
	PARSER_BLE_HEARD_STATE,
	PARSER_BLE_TYPE_STATE,
	PARSER_BLE_LEN_STATE,
	PARSER_BLE_DATA_STATE,
	PARSER_BLE_CRC_STATE,
	PARSER_BLE_TAIL_STATE,
};

//struct
uint32_t ble_timeout = 0;
uint8_t nus_handle_data_buff[10][360] = {0};
uint16_t nus_handle_data_length[10] = {0};
uint8_t nus_handle_data_rec_status[10] = {0};
uint8_t gate_mac[24] = {0};
extern unsigned char connect_flag;
	uint8_t toBuff_test[2048] = {0};
	uint8_t toBuff_test_flag = 0;
void transJsonPassthroughReport(uint8_t *data, uint16_t len)
{
	if(DEBUG) {
		transDebug(data, len+6);
	}
	uint8_t pass_data[600] = {0};
	json_object *jobj = json_object_new_object();
	json_object *jobj_comType = json_object_new_object();
	json_object *jobj_content = json_object_new_object();
	json_object *jobj_data = json_object_new_object();

	json_object_object_add(jobj_comType, "type", json_object_new_string("Up"));
	//4 bytes timestamp
	uint8_t buf_time[32] = {0};
	sprintf(buf_time, "%u", 1234567);
	json_object_object_add(jobj_comType, "msgId", json_object_new_string(buf_time));

	uint8_t clientId[24] = {0};
	//6 bytes mac
	sprintf(clientId,"%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", gate_mac[0], gate_mac[1], \
				gate_mac[2], gate_mac[3], gate_mac[4], gate_mac[5]);

	json_object_object_add(jobj_comType, "clientId", json_object_new_string(clientId));
	json_object_object_add(jobj_content, "response", json_object_new_string("Reply"));
	json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_PASSTHROUGH));
	
	uint8_t mac[40] = {0};
	sprintf(mac, "%02X%02X%02X%02X%02X%02X", data[5], data[4], data[3], data[2], data[1], data[0]);
	json_object_object_add(jobj_content, "mac", json_object_new_string(mac));
	transHexToAscii(pass_data, &data[6], len);

	
	
	json_object_object_add(jobj, TRANS_JOBJ_COMTYPE, jobj_comType);
	json_object_object_add(jobj_comType, TRANS_JOBJ_CONTENT, jobj_content);
	//json_object_object_add(jobj_content, TRANS_JOBJ_DATA, jobj_data);
	json_object_object_add(jobj_content, TRANS_JOBJ_DATA, json_object_new_string(pass_data));
	

	uint16_t json_len = strlen(json_object_to_json_string(jobj));
	if(1024>json_len) {
		memcpy(toBuff_test, json_object_to_json_string(jobj), json_len);
	} else {
		json_len = 0;
		LOGI("json buff overflow");
	}
	
	//free
	json_object_put(jobj_data);
	json_object_put(jobj_content);
	json_object_put(jobj_comType);
	json_object_put(jobj);
	if(DEBUG) {
			LOGI("transToJson quit");
	}
	//return json_len;
	toBuff_test_flag = 1;

}


void nus_data_report_pack_put(uint8_t *one_byte, uint8_t index)
{
	//testdata[4] = nus_handle_data_rec_status[index];
	switch(nus_handle_data_rec_status[index])
	{
		case PARSER_BLE_HEARD_STATE:
			if(*one_byte==0x68) {
				nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
				nus_handle_data_length[index]++;
				nus_handle_data_rec_status[index] = PARSER_BLE_TYPE_STATE;
			}
			break;
		case PARSER_BLE_TYPE_STATE:
			nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
			nus_handle_data_length[index]++;
			nus_handle_data_rec_status[index] = PARSER_BLE_LEN_STATE;
			break;
		case PARSER_BLE_LEN_STATE:
			nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
			nus_handle_data_length[index]++;
			if(nus_handle_data_length[index]==4) {
				nus_handle_data_rec_status[index] = PARSER_BLE_DATA_STATE;
			}
			break;
		case PARSER_BLE_DATA_STATE:
			nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
			nus_handle_data_length[index]++;
			if(nus_handle_data_length[index]==4+(nus_handle_data_buff[index][6+2]|(nus_handle_data_buff[index][6+3]<<8))) {
				nus_handle_data_rec_status[index] = PARSER_BLE_CRC_STATE;
			}
			if(DEBUG) {
				transDebug(nus_handle_data_buff[index], nus_handle_data_length[index]+6);
			}
			break;
		case PARSER_BLE_CRC_STATE:
			nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
			nus_handle_data_length[index]++;
			nus_handle_data_rec_status[index] = PARSER_BLE_TAIL_STATE;
			break;
		case PARSER_BLE_TAIL_STATE:
		{
			nus_handle_data_buff[index][6+nus_handle_data_length[index]] = *one_byte;
			nus_handle_data_length[index]++;
			nus_handle_data_rec_status[index] = PARSER_BLE_HEARD_STATE;

			transJsonPassthroughReport(nus_handle_data_buff[index], nus_handle_data_length[index]);
			memset(nus_handle_data_buff[index], 0, 320);
			nus_handle_data_length[index] = 0;
		}
			break;
		default:
			nus_handle_data_rec_status[index] = PARSER_BLE_HEARD_STATE;
			break;
	}
}

void nus_data_creat_report_pack(char *addr, uint8_t *data, uint32_t data_len)
{
	
	uint8_t i = 0;
	for(i=0; i<5; i++) {
		if(DEBUG) {
			transDebug(&addr[0], 6);
			transDebug(&nus_handle_data_buff[i][0], 6);
		}
		uint16_t ad_sum = addr[0]+addr[1]+addr[2]+addr[3]+addr[4]+addr[5];
		uint16_t nus_sum = nus_handle_data_buff[i][0]+nus_handle_data_buff[i][1]+nus_handle_data_buff[i][2]+nus_handle_data_buff[i][3]+nus_handle_data_buff[i][4]+nus_handle_data_buff[i][5];
		//if(addr[0]==nus_handle_data_buff[i][0] && addr[1]==nus_handle_data_buff[i][1] && addr[2]==nus_handle_data_buff[i][2] && addr[3]==nus_handle_data_buff[i][3] ) {
		//LOGI("%d==%d  %d", ad_sum, nus_sum, ad_sum==nus_sum);
		//LOGI("%d==%d  %d", addr[0], nus_handle_data_buff[i][0], addr[0]==nus_handle_data_buff[i][0]);
		transDebug((char *)&ad_sum, 2);
		transDebug((char *)&nus_sum, 2);
		if(nus_handle_data_buff[i][0]!=0) {
			for(uint8_t k=0; k<data_len; k++) {
				nus_data_report_pack_put(&data[k], i);
			}
			break;
		}
	}
	if(i==5) {  //first pack
		for(uint8_t j=0; j<5; j++) {
			if(nus_handle_data_buff[j][0] == 0 && \
				nus_handle_data_buff[j][1] == 0 && \
				nus_handle_data_buff[j][2] == 0 && \
				nus_handle_data_buff[j][3] == 0 && \
				nus_handle_data_buff[j][4] == 0 && \
				nus_handle_data_buff[j][5] == 0) {
				
				nus_handle_data_buff[j][0] = addr[0];
				nus_handle_data_buff[j][1] = addr[1];
				nus_handle_data_buff[j][2] = addr[2];
				nus_handle_data_buff[j][3] = addr[3];
				nus_handle_data_buff[j][4] = addr[4];
				nus_handle_data_buff[j][5] = addr[5];
				nus_handle_data_length[j] = 0;
				for(uint8_t k=0; k<data_len; k++) {
					nus_data_report_pack_put(&data[k], j);
				}
				break;
			}
		}
	}
}

/*****************************************************************************************************
@function:      static void transJsonPassthrough(json_object *jobj, uint8_t *data)
@description:  	convert Passthrough hexadecimal data to json data
@param:			jobj	:output josn object
				data	:data
@return:        none
******************************************************************************************************/
static uint8_t transJsonPassthrough(json_object *jobj, uint8_t *data)
{
	uint16_t len = 0;
	uint8_t mac[48] = {0};
	uint8_t pass_data[600] = {0};
	if(data[0]==0x40) {
		
		len = ((data[8]<<8)|data[7]) - 7;
		
		
		if(data[9]==0) {
			json_object_object_add(jobj, "response", json_object_new_string("Ok"));
		}else if(data[9]==1){
			json_object_object_add(jobj, "response", json_object_new_string("Fail"));
		}
		sprintf(mac, "%02X%02X%02X%02X%02X%02X", data[15], data[14], data[13], data[12], data[11], data[10]);
		json_object_object_add(jobj, "mac", json_object_new_string(mac));
		transHexToAscii(pass_data, &data[16], len);
		json_object_object_add(jobj, TRANS_JOBJ_DATA, json_object_new_string(pass_data));
		return 0;
	} else if(data[0]==0) {
		ble_timeout = 0;
		json_object_object_add(jobj, "response", json_object_new_string("Reply"));
		
		len = ((data[8]<<8)|data[7]) - 6;
		if(DEBUG) {
			transDebug(&data[15], len);
		}
		//不在路由组包,在主机内组包
		//nus_data_creat_report_pack(&data[9], &data[15], len);
		replyPassthrough(&data[9], &data[15], len, 0);
		
		sprintf(mac, "%02X%02X%02X%02X%02X%02X", data[14], data[13], data[12], data[11], data[10], data[9]);
		json_object_object_add(jobj, "mac", json_object_new_string(mac));
		transHexToAscii(pass_data, &data[15], len);
		json_object_object_add(jobj, TRANS_JOBJ_DATA, json_object_new_string(pass_data));
		return 0;
	}
}


/*****************************************************************************************************
@function:      static void transJsonUpdateTime(json_object *jobj, uint8_t *data)
@description:   convert Update time hexadecimal data to json data
@param:			jobj	:output josn object
				data	:data
@return:        none
******************************************************************************************************/
static void transJsonUpdateTime(json_object *jobj, uint8_t *data)
{
	if(data[2]==0) {
		json_object_object_add(jobj, "response", json_object_new_string("Ok"));
	}else if(data[2]==1){
		json_object_object_add(jobj, "response", json_object_new_string("Fail"));
	}
}


/*****************************************************************************************************
@function:      static void transJsonFirmwareUpdate(json_object *jobj, uint8_t *data)
@description:   convert Firmware update hexadecimal data to json data
@param:			jobj	:output josn object
				data	:data
@return:        none
******************************************************************************************************/
static void transJsonFirmwareUpdate(json_object *jobj, uint8_t *data)
{
	switch(data[2]) {
		case 0:
			json_object_object_add(jobj, "step", json_object_new_string("0"));
			if(data[3]==0) {
				json_object_object_add(jobj, "response", json_object_new_string("ReadyOk"));
			}else if(data[3]==1) {
				json_object_object_add(jobj, "response", json_object_new_string("UnsupportedType"));
			}else if(data[3]==2) {
				json_object_object_add(jobj, "response", json_object_new_string("VersionLatest"));
			}else if(data[3]==3) {
				json_object_object_add(jobj, "response", json_object_new_string("SizeError"));
			}else if(data[3]==4) {
				json_object_object_add(jobj, "response", json_object_new_string("NameError"));
			}else {
				json_object_object_add(jobj, "response", json_object_new_string("Error"));
			}
			break;
/*		case 1:
			json_object_object_add(jobj, "step", json_object_new_string("1"));
			if(data[3]==0) {
				json_object_object_add(jobj, "response", json_object_new_string("StartDownload"));
			}else if(data[3]==1) {
				json_object_object_add(jobj, "response", json_object_new_string("404NotFound"));
			}else if(data[3]==2) {
				json_object_object_add(jobj, "response", json_object_new_string("FileNotFound"));
			}else {
				json_object_object_add(jobj, "response", json_object_new_string("Error"));
			}
			break;
		case 2:
			json_object_object_add(jobj, "step", json_object_new_string("2"));
			break;
		case 3:
			json_object_object_add(jobj, "step", json_object_new_string("3"));
			break;
		case 4:
			json_object_object_add(jobj, "step", json_object_new_string("4"));
			if(data[3]==0) {
				json_object_object_add(jobj, "status", json_object_new_string("Ok"));
			}else {
				json_object_object_add(jobj, "status", json_object_new_string("Error"));
			}
			break;*/
		default:
			json_object_object_add(jobj, "step", json_object_new_string("Error"));
			json_object_object_add(jobj, "response", json_object_new_string("Error"));
			break;
		
	}
}

/*****************************************************************************************************
@function:      static void transJsonException(json_object *jobj, uint8_t *data)
@description:   deal with exception reply
@param:			jobj	:output
				data	:exception data
@return:        none
******************************************************************************************************/
static void transJsonException(json_object *jobj, uint8_t *data)
{
	if(data[2]==1) {
		json_object_object_add(jobj, "execption", json_object_new_string("ErrorCheck"));
	} else if(data[2]==2){
		json_object_object_add(jobj, "execption", json_object_new_string("ErrorContent"));
	} else if(data[2]==3){
		json_object_object_add(jobj, "execption", json_object_new_string("NotFunction"));
	} else if(data[2]==4){
		json_object_object_add(jobj, "execption", json_object_new_string("Nonsupport"));
	}
}

/*****************************************************************************************************
@function:      extern uint16_t transToJson(uint8_t *send_sock_buf, uint32_t send_sock_buf_len, uint8_t *toBuff, uint16_t tobuff_max_len)
@description:   Convert hexadecimal data to json data
@param:			send_sock_buf		:hexadecimal data
				send_sock_buf_len	:hexadecimal data len
				toBuff				:output json string data
				tobuff_max_len		:toBuff max size, check array index out of bounds
@return:        toBuff lenght
******************************************************************************************************/
extern uint16_t transToJson(uint8_t *send_sock_buf, uint32_t send_sock_buf_len, uint8_t *toBuff, uint16_t tobuff_max_len)
{
	if(DEBUG) {
		transDebug(send_sock_buf, send_sock_buf_len);
	}
	
	//Fixed bytes 00 BC 61 4E + length 00 00 01 2A
	send_sock_buf += 8;
	send_sock_buf_len -= 8;
	
	json_object *jobj = json_object_new_object();
	json_object *jobj_comType = json_object_new_object();
	json_object *jobj_content = json_object_new_object();
	json_object *jobj_data = json_object_new_object();

	//1 bytes header
	if(send_sock_buf[0] != 0x2A) {
		json_object_put(jobj);
		json_object_put(jobj_comType);
		json_object_put(jobj_content);
		json_object_put(jobj_data);
 		if(DEBUG) {
			LOGI("Not find pack header 0x2A\n");
		}
		return 0;
	}
	json_object_object_add(jobj_comType, "type", json_object_new_string("Up"));
	//4 bytes timestamp
	uint8_t buf_time[32] = {0};
	sprintf(buf_time, "%u", *(uint32_t *)&send_sock_buf[1]);
	json_object_object_add(jobj_comType, "msgId", json_object_new_string(buf_time));

	uint8_t clientId[24] = {0};
	//6 bytes mac
	sprintf(clientId,"%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", send_sock_buf[14], send_sock_buf[13], \
				send_sock_buf[12], send_sock_buf[11], send_sock_buf[10], send_sock_buf[9]);
	gate_mac[0] = send_sock_buf[14];
	gate_mac[1] = send_sock_buf[13];
	gate_mac[2] = send_sock_buf[12];
	gate_mac[3] = send_sock_buf[11];
	gate_mac[4] = send_sock_buf[10];
	gate_mac[5] = send_sock_buf[9];
	

/*	srand((unsigned)time(NULL));
	clientId[12] = rand()%10 + '0';
	clientId[13] = rand()%10 + '0';
	clientId[14] = rand()%10 + '0';
	clientId[15] = '\0';*/
	json_object_object_add(jobj_comType, "clientId", json_object_new_string(clientId));

	if(DEBUG) {
		LOGI("EXCEP Code:%02X\n", send_sock_buf[5]);
	}
	///2A 45 15 00 00 00 02 43 80 02 1A 00 95 D4 8C 78 00 03 00 11 D2 F9 F1 59 34 CE 4F 02 01 06 0
	if(0x43 == send_sock_buf[7])
	{
		LOGI("heartbeat######");
		if(send_sock_buf[15] != 1)
		{
			send_sock_buf[7] = frame_functype_buff;
			if(DEBUG) 
			{
				LOGI("trans to %x", frame_functype_buff);
			}
		}
	}
	
	//function code
	switch(send_sock_buf[7])
		{
			case CODE_PASSTHROUGH:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_PASSTHROUGH));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					if(transJsonPassthrough(jobj_data, &send_sock_buf[8])) {
						json_object_put(jobj_data);
						json_object_put(jobj_content);
						json_object_put(jobj_comType);
						json_object_put(jobj);
						return 0;
					}
				}
				break;
			case CODE_TIME_UPDATE:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_TIMEUPDATE));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonUpdateTime(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_CONNECT_DEVICE_SHORT:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_CONNECTDEVICE));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonConnectDevice(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_DISCONNECT_DEVICE:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_DISCONNECTDEVICE));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonDisconnectDevice(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_QUERY_DEVICE_STATUS:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_DEVICESTATUS));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonQueryDeviceStatus(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_QUERY_CONNECTED_NUM:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_QUERYCONECTED));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonQueryConnected(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_START_SCAN:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_STARTSCAN));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonStartScan(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_STOP_SCAN:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_STOPSACN));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonStopScan(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_QUERY_GATEWAY_FIRMWARE:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_FIRMWAREVERSION));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonQueryGatewayVersion(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_GATEWAY_FIRMWARE_UPDATE:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_FIRMWAREUPDATE));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonFirmwareUpdate(jobj_data, &send_sock_buf[15]);
				}
				break;
//			case CODE_DEVICE_FIRMWARE_UPDATE:
//				json_object_object_add(jobj_content, "type", json_object_new_string("NULL"));
//				//404 not found

			
//				break;
			case CODE_CONNECT_DEVICE_LONG:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_CONNECTDEVICELONG));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonConnectDeviceLong(jobj_data, &send_sock_buf[15]);
				}
				break;
//			case CODE_TEPORT_MATTER:
//				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_REPORTMATTER));
//				if(send_sock_buf[5]&0x80) {
//					transJsonException(jobj_data, &send_sock_buf[15]);
//				} else {
//					transJsonReportMatter(jobj_data, &send_sock_buf[15]);
//				}
//				break;
			case CODE_REBOOT:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_REBOOT));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					if(send_sock_buf[17]==0) {
						json_object_object_add(jobj_data, "response", json_object_new_string("Ok"));
					}else if(send_sock_buf[17]==1) {
						json_object_object_add(jobj_data, "response", json_object_new_string("Fail"));
					}else {
						json_object_object_add(jobj_data, "response", json_object_new_string("Error"));
					}
				}
				break;
			case CODE_SCAN_ARG_CONFIG:
				exec_logstring_command("CODE_SCAN_ARG_CONFIG");
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_SCANCONFIG));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonScanConfig(jobj_data, &send_sock_buf[15]);
				}
				break;
			case CODE_SCAN_ARG_QUERY:
				exec_logstring_command("CODE_SCAN_ARG_QUERY");
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_SACNQUERY));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonScanQuery(jobj_data, &send_sock_buf[15]);
				}				
				break;
			case CODE_QUERY_FIRMWARE:
				exec_logstring_command("CODE_QUERY_FIRMWARE");
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_QUERYFIRMWARE));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonQueryFirmware(jobj_data, &send_sock_buf[15]);
				}				
				break;
			case CODE_WARING:
				exec_logstring_command("CODE_WARING");
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_WARING));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					transJsonWaring(jobj_data, &send_sock_buf[15]);
				}			
				break;
			case CODE_FACTORY_RESET:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_FACTORYTESET));
				if(send_sock_buf[5]&0x80) {
					transJsonException(jobj_data, &send_sock_buf[15]);
				} else {
					if(send_sock_buf[17]==0) {
						json_object_object_add(jobj_data, "response", json_object_new_string("Ok"));
					}else if(send_sock_buf[17]==1) {
						json_object_object_add(jobj_data, "response", json_object_new_string("Fail"));
					}else {
						json_object_object_add(jobj_data, "response", json_object_new_string("Error"));
					}
				}
				break;
			case CODE_ONLINE:
			{
				
				//json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_ONLINE));
				//json_object_object_add(jobj_data, "notify", json_object_new_string(TRANS_STRING_ONLINE));
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_REPORTBORDCASTOTHER));
				transJsonBordcast(jobj_data, &send_sock_buf[15], false);
				frame_functype_buff = 0x40;
				break;
			}
			case CODE_REPORT_BORDCAT:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_REPORTBORDCAST));
				transJsonBordcast(jobj_data, &send_sock_buf[15], true);
				frame_functype_buff = 0x41;
				break;
			case CODE_REPORT_BORDCAT_ALL:
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_REPORTBORDCASTALL));
				transJsonBordcast(jobj_data, &send_sock_buf[15], false);
				frame_functype_buff = 0x42;
				break;
			case CODE_HEARTBEAT:
			{
				if(0x43 == send_sock_buf[7] && (send_sock_buf[15] != 1))
				{
					send_sock_buf[7] = 0x40;
					if(DEBUG)
					{
						LOGI("in case CODE_HEARTBEAT error");
					}
					transJsonBordcast(jobj_data, &send_sock_buf[15], false);
				}
				json_object_object_add(jobj_content, "type", json_object_new_string(TRANS_STRING_HEARTBEAT));
				break;
			}
			default:
				break;
		}

	json_object_object_add(jobj, TRANS_JOBJ_COMTYPE, jobj_comType);
	json_object_object_add(jobj_comType, TRANS_JOBJ_CONTENT, jobj_content);
	json_object_object_add(jobj_content, TRANS_JOBJ_DATA, jobj_data);

	uint16_t json_len = strlen(json_object_to_json_string(jobj));
	if(tobuff_max_len>json_len) {
		memcpy(toBuff, json_object_to_json_string(jobj), json_len);
	} else {
		json_len = 0;
		LOGI("json buff overflow");
	}
	
	//free
	json_object_put(jobj_data);
	json_object_put(jobj_content);
	json_object_put(jobj_comType);
	json_object_put(jobj);
	if(DEBUG) {
			LOGI("transToJson quit");
	}
	return json_len;
	
}


/*****************************************************************************************************
@function:      static uint16_t transHexCRC(uint8_t *data, uint16_t len)
@description:   Calculate check value
@param:			data	:hex data
				len		:data len
@return:        The checksum
******************************************************************************************************/
static uint16_t transHexCRC(uint8_t *data, uint16_t len)
{
	uint16_t crc=0;
	for(uint16_t i=0; i<len; i++) {
			crc += data[i];
	}
	return crc;
}


/*****************************************************************************************************
@function:      static int string_to_json(char *string, unsigned long len, json_object **json)
@description:   Convert the string to a json object
@param:			string	:intput string
				len		:string len
				json	:output object
@return:        none
******************************************************************************************************/
int string_to_json(char *string, unsigned long len, json_object **json)
{
    json_tokener *tok = json_tokener_new();//创建一个json_tokener对象，以便进行转换,记得要释放
    enum json_tokener_error jerr;
    do
    {
        *json = json_tokener_parse_ex(tok, string, len);
    }while((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
	
    if(jerr != json_tokener_success  || tok->char_offset < (int)len)
    {
        json_tokener_free(tok);
        return -1;
    }
	
    if(json_object_get_type(*json) == json_type_object)
    {
        json_tokener_free(tok);
        return 0;
    }
    json_tokener_free(tok);
    return -1;
}


/*****************************************************************************************************
@function:      static uint16_t transHexHeader(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   jobj 'comType' data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexHeader(TRANS_JOBJ_t *jobj_t, uint8_t *toBuff)
{
	uint16_t index = 0;
	
	//header
	if(strncmp("Down",  gjson_get_string(jobj_t->jobj_comType, "type"), 5)==0) {

	}
	toBuff[index++] = uc_pack_header;

	//timestamp
	uint32_t timestamp = gjson_get_int(jobj_t->jobj_comType, "msgId");
	toBuff[index++] = (timestamp>>0)&0xff;
	toBuff[index++] = (timestamp>>8)&0xff;
	toBuff[index++] = (timestamp>>16)&0xff;
	toBuff[index++] = (timestamp>>24)&0xff;

	//reserved
	toBuff[index++] = 0;
	//version
	toBuff[index++] = protocol_version;

	//type
	index++;
	index++;
	index = 9;
	
	uint8_t mac[48] = {0};
	//transCenterGetLanMac(mac);
	memcpy(mac, get_mac(true), 12);
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[14], &toBuff[13], \
		&toBuff[12], &toBuff[11], &toBuff[10], &toBuff[9]);
	//toBuff[9] = 0xd7;
	//toBuff[10] = 0xCC;
	//toBuff[11] = 0xCB;
	//toBuff[12] = 0xCA;
	//toBuff[13] = 0xC9;
	//toBuff[14] = 0xC8;
	index = 15;
	if(DEBUG) {
		LOGI("transHeader");
		transDebug(toBuff, 15);
	}
	
	//json_object_put(jobj_content);
	//json_object_put(jobj_comType);
	return 15;
}


/*****************************************************************************************************
@function:      static uint16_t transHexOnline(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexOnline(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x40;
	toBuff[8] = 0xC0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x02;
	toBuff[index++] = 0x00;

	//data
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexOline(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        tobuff lenght
******************************************************************************************************/
static uint16_t transHexOline(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x40;
	toBuff[8] = 0xC0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x02;
	toBuff[index++] = 0x00;
	
	toBuff[index++] = 0x00;

	if(strncmp("Ok", gjson_get_string(jobj->jobj_data, "response"), strlen("Ok"))==0) {
		toBuff[index++] = 0x00;
	} else if(strncmp("Fail", gjson_get_string(jobj->jobj_data, "response"), strlen("Fail"))==0) {
		toBuff[index++] = 0x01;
	}
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;

}

/*****************************************************************************************************
@function:      static uint16_t transHexReboot(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexReboot(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x0D;
	toBuff[8] = 0xA0;

	toBuff[15] = 0;
	toBuff[16] = 0;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, 17);
	toBuff[17] = crc & 0xFF;
	toBuff[18] = (crc>>8) & 0XFF;
	toBuff[19] = uc_pack_tail;
	return 20;
}


/*****************************************************************************************************
@function:      static uint16_t transHexReportMatter(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexReportMatter(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_string(jobj->jobj_data, "matter") == NULL) {
		return 0;
	}
	//type
	toBuff[7] = 0x0C;
	toBuff[8] = 0xA0;

	uint16_t index = 15;
	toBuff[index++] = 1;
	toBuff[index++] = 0;

	char mbuf[48] = {0};
	memcpy(mbuf, gjson_get_string(jobj->jobj_data, "matter"), strlen(gjson_get_string(jobj->jobj_data, "matter")));
	if(DEBUG) {
		LOGI("matter: %s", mbuf);
	}
	if(strncmp("Open", mbuf, strlen("Open"))==0) {
		toBuff[index++] = 0;
	} else if(strncmp("Close", mbuf, strlen("Close"))==0) {
		toBuff[index++] = 1;
	} else if(gjson_get_int(jobj->jobj_data, "matter") > 0) {
		toBuff[15] = 2;
		toBuff[index++] = 2;
		toBuff[index++] = gjson_get_int(jobj->jobj_data, "matter");
	} else if(strncmp("Status", mbuf, strlen("Status"))==0) {
		toBuff[index++] = 3;
	} else if(strncmp("Gettime", mbuf, strlen("Gettime"))==0) {
		toBuff[index++] = 4;
	} else {
		return 0;
	}
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexFactoryTeset(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexFactoryTeset(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x0E;
	toBuff[8] = 0xA0;
	
	toBuff[15] = 0;
	toBuff[16] = 0;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, 17);
	toBuff[17] = crc & 0xFF;
	toBuff[18] = (crc>>8) & 0XFF;
	toBuff[19] = uc_pack_tail;
	return 20;
}


/*****************************************************************************************************
@function:      static uint16_t transHexTimeUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexTimeUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_int(jobj->jobj_data, "time")<0) {
		return 0;
	}
	uint16_t index;
	//type
	toBuff[7] = 0x01;
	toBuff[8] = 0xA0;
	
	index = 15;

	//len
	toBuff[index++] = 0x04;
	toBuff[index++] = 0x00;

	//data
	uint32_t utc = 0;
	if(gjson_get_int(jobj->jobj_data, "time") < 0) {
		return 0;
	}
	utc = gjson_get_int(jobj->jobj_data, "time");
	toBuff[index++] = (utc>>24)&0xff;
	toBuff[index++] = (utc>>16)&0xff;
	toBuff[index++] = (utc>>8)&0xff;
	toBuff[index++] = (utc>>0)&0xff;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexPassthrough(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexPassthrough(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	uint16_t index;
	//type
	toBuff[7] = 0x00;
	toBuff[8] = 0x20;
	
	index = 17;
	//mac
	if(gjson_get_string(jobj->jobj_data, "mac")==NULL) {
		return 0;
	}
	const char *mac = gjson_get_string(jobj->jobj_data, "mac");
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[index+5], &toBuff[index+4], \
		&toBuff[index+3], &toBuff[index+2], &toBuff[index+1], &toBuff[index+0]);
	index += 6;
	
	//data
	const char *data = gjson_get_string(jobj->jobj_data, "data");
	//LOGI("%s", data);
	uint16_t stlen = strlen(data);
	if(transAsiicToHex((uint8_t *)data, &toBuff[index], stlen)) {
		index += stlen/2;
	} else {
		return 0;
	}

	uint16_t len = index-17;
	toBuff[15] = len&0xFF;
	toBuff[16] = (len>>8)&0xff;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}

/*****************************************************************************************************
@function:      static uint16_t transHexScanConfig(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexScanConfig(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	uint16_t index;
	//type
	toBuff[7] = 0x13;
	toBuff[8] = 0xA0;
	
	index = 15;
	//len
	toBuff[index++] = 0x04;
	toBuff[index++] = 0x00;

	const char *switchs = gjson_get_string(jobj->jobj_data, "switch");
	const char *mode = gjson_get_string(jobj->jobj_data, "mode");
//	const char *cycle = gjson_get_string(jobj->jobj_data, "cycle");
	const char *filter = gjson_get_string(jobj->jobj_data, "filter");
	if(strncmp(switchs, "Open", 4)==0) {
		toBuff[index++] = 0x01;
	} else if(strncmp(switchs, "Close", 5)==0) {
		toBuff[index++] = 0x00;
	} else {
		return 0;
	}
	
	if(strncmp(mode, "Single", 6)==0) {
		toBuff[index++] = 0x00;
	} else if(strncmp(mode, "Keep", strlen("Keep"))==0) {
		toBuff[index++] = 0x01;
	} else {
		return 0;
	}
	
	if(gjson_get_int(jobj->jobj_data, "cycle")==-1) {
		return 0;
	}
	toBuff[index++] = gjson_get_int(jobj->jobj_data, "cycle");
	
	if(strncmp(filter, "True", 4)==0) {
		toBuff[index++] = 0x00;
		frame_functype_buff = 0x41;
	} else if(strncmp(filter, "False", 5)==0) {
		toBuff[index++] = 0x01;
		frame_functype_buff = 0x42;
	} else if(strncmp(filter, "Other", 5)==0) {
		toBuff[index++] = 0x02;
		frame_functype_buff = 0x40;
	} else {
		return 0;
	}
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}

/*****************************************************************************************************
@function:      static uint16_t transHexScanQuery(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexScanQuery(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	uint16_t index;
	//type
	toBuff[7] = CODE_SCAN_ARG_QUERY;
	toBuff[8] = 0xA0;
	
	index = 15;
	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}

/*****************************************************************************************************
@function:      static uint16_t transHexQueryFirmware(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexQueryFirmware(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	uint16_t index;
	
	toBuff[7] = CODE_QUERY_FIRMWARE;
	toBuff[8] = 0xA0;
	
	index = 15;
	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}

/*****************************************************************************************************
@function:      static uint16_t transHexWaring(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexWaring(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	uint16_t index;
	
	uint8_t waringType = gjson_get_int(jobj->jobj_data, "waringType");
	
	toBuff[7] = CODE_WARING;
	toBuff[8] = 0xA0;
	
	index = 15;
	//len
	toBuff[index++] = 0x01;
	toBuff[index++] = 0x00;
	
	toBuff[index++] = waringType;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}

/*****************************************************************************************************
@function:      static uint16_t transHexConnectDeviceLong(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexConnectDeviceLong(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_string(jobj->jobj_data, "mac") == NULL) {
		return 0;
	}
	//type
	toBuff[7] = 0x0B;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x06;
	toBuff[index++] = 0x00;

	uint8_t mac[20] = {0};
	memcpy(mac, gjson_get_string(jobj->jobj_data, "mac"), 12);
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[index+5], &toBuff[index+4], \
		&toBuff[index+3], &toBuff[index+2], &toBuff[index+1], &toBuff[index+0]);
	//data
	index += 6;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexFirmwareVersion(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexFirmwareVersion(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x08;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexFirmwareUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexFirmwareUpdate(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x09;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;

	
	if(gjson_get_int(jobj->jobj_data, "step")>=0) {
		toBuff[index++] = gjson_get_int(jobj->jobj_data, "step");
	} else {
		return 0;
	}
	const char *firmwareType = gjson_get_string(jobj->jobj_data, "firmwareType");

	if(firmwareType==NULL) {
		return 0;
	} else if(strncmp("App", firmwareType, strlen("App"))==0) {
		toBuff[index++] = 0;
	} else if(strncmp("BootLoader", firmwareType, strlen("BootLoader"))==0) {
		toBuff[index++] = 1;
	} else if(strncmp("SoftDevice", firmwareType, strlen("SoftDevice"))==0) {
		toBuff[index++] = 2;
	} else {
		return 0;
	}

	uint8_t fversion = gjson_get_int(jobj->jobj_data, "firmwareVersion");
	if(fversion>=0) {
		toBuff[index++] = fversion & 0xff;
		toBuff[index++] = (fversion>>8) & 0xff;
	} else {
		return 0;
	}

	uint32_t fsize = gjson_get_int(jobj->jobj_data, "firmwareSize");
	if(fsize>0) {
		toBuff[index++] = fsize & 0xff;
		toBuff[index++] = (fsize>>8) & 0xff;
		toBuff[index++] = (fsize>>16) & 0xff;
		toBuff[index++] = (fsize>>24) & 0xff;
	} else {
		return 0;
	}
	
	uint16_t fport = gjson_get_int(jobj->jobj_data, "port");
	if(fport>0) {
		toBuff[index++] = fport & 0xff;
		toBuff[index++] = (fport>>8) & 0xff;
	} else {
		return 0;
	}

	const char *name = gjson_get_string(jobj->jobj_data, "name");

	if(name==NULL) {
		return 0;
	}

	uint8_t fnamesize = strlen(name);
	toBuff[index++] = fnamesize;
	for(uint8_t i=0; i<fnamesize; i++) {
		toBuff[index++] = name[i];
	}
	
	uint16_t len = index-17;

	//len
	toBuff[15] = len & 0xff;
	toBuff[16] = (len>>8) & 0xff;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexStopScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexStopScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x07;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexStartScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexStartScan(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x06;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexQueryConnected(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexQueryConnected(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	//type
	toBuff[7] = 0x05;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x00;
	toBuff[index++] = 0x00;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexDeviceStatus(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexDeviceStatus(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_string(jobj->jobj_data, "mac") == NULL) {
		return 0;
	}
	//type
	toBuff[7] = 0x04;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x06;
	toBuff[index++] = 0x00;
	
	uint8_t mac[16] = {0};
	memcpy(mac, gjson_get_string(jobj->jobj_data, "mac"), 12);
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[index+5], &toBuff[index+4], \
		&toBuff[index+3], &toBuff[index+2], &toBuff[index+1], &toBuff[index+0]);
	//data
	index += 6;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexDisconnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexDisconnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_string(jobj->jobj_data, "mac") == NULL) {
		return 0;
	}
	//type
	toBuff[7] = 0x03;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x06;
	toBuff[index++] = 0x00;
	
	uint8_t mac[16] = {0};
	memcpy(mac, gjson_get_string(jobj->jobj_data, "mac"), 12);
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[index+5], &toBuff[index+4], \
		&toBuff[index+3], &toBuff[index+2], &toBuff[index+1], &toBuff[index+0]);
	//data
	index += 6;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}


/*****************************************************************************************************
@function:      static uint16_t transHexConnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
@description:   Json data is converted to hexadecimal
@param:         jobj	:json object;
				toBuff	:output data
@return:        toBuff lenght
******************************************************************************************************/
static uint16_t transHexConnectDevice(TRANS_JOBJ_t *jobj, uint8_t *toBuff)
{
	if(gjson_get_string(jobj->jobj_data, "mac") == NULL) {
		return 0;
	}
	//type
	toBuff[7] = 0x02;
	toBuff[8] = 0xA0;

	uint16_t index = 15;

	//len
	toBuff[index++] = 0x06;
	toBuff[index++] = 0x00;
	
	uint8_t mac[48] = {0};
	memcpy(mac, gjson_get_string(jobj->jobj_data, "mac"), strlen(gjson_get_string(jobj->jobj_data, "mac")));
	if(DEBUG) {
		LOGI("get device mac:%s", mac);
	}
	sscanf(mac, "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX", &toBuff[index+5], &toBuff[index+4], \
		&toBuff[index+3], &toBuff[index+2], &toBuff[index+1], &toBuff[index+0]);
	//data
	index += 6;
	
	uint16_t crc;
	crc = transHexCRC(toBuff, index);
	toBuff[index++] = crc & 0xFF;
	toBuff[index++] = (crc>>8) & 0XFF;
	toBuff[index++] = uc_pack_tail;
	return index;
}




typedef struct {
	uint8_t *cmd;
	struct mosquitto *mosq;
}TRANSUPBUF_t;

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
	struct mosquitto *mosq = cmd_t->mosq;
	for(int i=0; i<5; i++) {
		LOGI("The router will start the update in %d seconds!", 5-i);
		sleep(1);
	}
	uint8_t value[512];
	memset(value, 0, sizeof(value));
	
	//LOGI(cmd_t->cmd);
	//exec_command(cmd_t->cmd);
	command_output(cmd_t->cmd, value);
	LOGI("%s",value);

	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&jobj_t);

	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string("Upgrade"));
	
	if(strstr(value, "Image file not found")==NULL) {
		//succesful, but not run here, starting the upgrade
	} else {
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("404"));
	}
	
	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);	

	const char *js_str = json_object_to_json_string(jobj_t.jobj);
	mqttMessageSend(mosq, (char *)js_str, strlen(js_str));
	
	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);

	return 0;
}



/*****************************************************************************************************
@function:      static uint16_t transUpgrade(TRANS_JOBJ_t *jobj_t, uint8_t *toBuff, sock_ctx_t *sock_ctx)
@description:   update routeing firmware
@param:         jobj_t:		json object;
				toBuff:		not used;
				sock_ctx:	Main transfer file descriptor used to send data
@return:        0, zero
******************************************************************************************************/
static uint16_t transUpgrade(TRANS_JOBJ_t *jobj_t, uint8_t *toBuff, struct mosquitto *mosq)
{
	int rc = 0;
	static char upgrade_cmd[512] = {0};
	static TRANSUPBUF_t trans_cmd;
	trans_cmd.cmd = upgrade_cmd;
	trans_cmd.mosq = mosq;
	
	memset(upgrade_cmd, 0, sizeof(upgrade_cmd));
	
	const char *url = gjson_get_string(jobj_t->jobj_data, "url");
	int version = gjson_get_int(jobj_t->jobj_data, "version");

	// Get local version
	struct uci_context *ctx = guci2_init();
	char local_ver[32] = {0};

	guci2_get(ctx, "cositea.firmware.version", local_ver);

	if (ctx) {
		guci2_free(ctx);
	}
	

	uint8_t clientId[24] = {0};
	uint8_t mac[48] = {0};
	memcpy(clientId, get_mac(true), 12);
	srand((unsigned)time(NULL));
	clientId[12] = rand()%10 + '0';
	clientId[13] = rand()%10 + '0';
	clientId[14] = rand()%10 + '0';
	clientId[15] = '\0';
	gjson_set_string(jobj_t->jobj_comType, "type", "Up");
	gjson_set_string(jobj_t->jobj_comType, "clientId", clientId);

	// Compare firmware version with remote
	if (version <= atoi(local_ver)) {
		json_object_object_add(jobj_t->jobj, "response", json_object_new_string("Latest"));
		const char *js_str = json_object_to_json_string(jobj_t->jobj);
		mqttMessageSend(mosq, (char *)js_str, strlen(js_str));
		return 0;
	}
	if(DEBUG) {
		exec_command("logger sysupgrade router");
	}
	
	if(strncmp("1", gjson_get_string(jobj_t->jobj_data, "keepSetting"), strlen("1"))==0) {
		sprintf(upgrade_cmd, "sleep 3;sysupgrade -n %s", url);
	} else 
		sprintf(upgrade_cmd, "sleep 3;sysupgrade %s", url);{
	}
	pthread_t ntid;
	int err = pthread_create(&ntid, NULL, pthreadUpgrade, &trans_cmd);
	if(err==0) {
		json_object_object_add(jobj_t->jobj_data, "response", json_object_new_string("ReadyUpdate"));

		const char *js_str = json_object_to_json_string(jobj_t->jobj);
		mqttMessageSend(mosq, (char *)js_str, strlen(js_str));
		return 0;
	} else {
		json_object_object_add(jobj_t->jobj_data, "response", json_object_new_string("Fail"));
		
		const char *js_str = json_object_to_json_string(jobj_t->jobj);
		mqttMessageSend(mosq, (char *)js_str, strlen(js_str));
		return 0;
	}
}


#define jsonStrTypeCmp(json_content, str) strncmp(str, gjson_get_string(json_content, "type"), strlen(str))

/*****************************************************************************************************
@function:      extern uint16_t transJsonToHex(uint8_t *json_data, uint32_t json_data_len, uint8_t *toBuff, sock_ctx_t *sock_ctx)
@description:   Convert json data to hexadecimal data
@param:         json_data:		input data,Json format string;
				json_data_len:	input data lenght;
				toBuff:			output data buff;
				sock_ctx:		Main transfer file descriptor used to send data
@return:        toBuff lenght
******************************************************************************************************/
extern uint16_t transJsonToHex(uint8_t *json_data, uint32_t json_data_len, uint8_t *toBuff, struct mosquitto *mosq)
{
	TRANS_JOBJ_t jobj_t;
	uint16_t hex_len = 0;
	if(string_to_json(json_data, json_data_len, &jobj_t.jobj)==0) {
		//LOGI("string to json success\n");
	} else {
		LOGI("string to json fail\n");
		return 0;
	}
	jobj_t.jobj_comType = gjson_get_object(jobj_t.jobj, TRANS_JOBJ_COMTYPE);
	jobj_t.jobj_content = gjson_get_object(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT);
	jobj_t.jobj_data = gjson_get_object(jobj_t.jobj_content, TRANS_JOBJ_DATA);
	transHexHeader(&jobj_t, toBuff);
	if(strncmp(TRANS_STRING_ONLINE, gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_ONLINE))==0) {
		hex_len = transHexOline(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_FACTORYTESET,     gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_FACTORYTESET))==0) {
		hex_len = transHexFactoryTeset(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_REBOOT,           gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_REBOOT))==0) {
		hex_len = transHexReboot(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_REPORTMATTER,     gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_REPORTMATTER))==0) {
		hex_len = transHexReportMatter(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_CONNECTDEVICELONG, gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_CONNECTDEVICELONG))==0) {
		hex_len = transHexConnectDeviceLong(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_FIRMWAREVERSION,  gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_FIRMWAREVERSION))==0) {
		hex_len = transHexFirmwareVersion(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_FIRMWAREUPDATE,   gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_FIRMWAREUPDATE))==0) {
		hex_len = transHexFirmwareUpdate(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_STOPSACN,         gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_STOPSACN))==0) {
		hex_len = transHexStopScan(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_STARTSCAN,        gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_STARTSCAN))==0) {
		hex_len = transHexStartScan(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_QUERYCONECTED,    gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_QUERYCONECTED))==0) {
		hex_len = transHexQueryConnected(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_DEVICESTATUS,     gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_DEVICESTATUS))==0) {
		hex_len = transHexDeviceStatus(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_DISCONNECTDEVICE, gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_DISCONNECTDEVICE))==0) {
		hex_len = transHexDisconnectDevice(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_CONNECTDEVICE,    gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_CONNECTDEVICE))==0) {
		hex_len = transHexConnectDevice(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_TIMEUPDATE,       gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_TIMEUPDATE))==0) {
		hex_len = transHexTimeUpdate(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_PASSTHROUGH,      gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_PASSTHROUGH))==0) {
		hex_len = transHexPassthrough(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_SYSUPGRADE,       gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_SYSUPGRADE))==0) {
		transUpgrade(&jobj_t, toBuff, mosq);
	} else if(strncmp(TRANS_STRING_SCANCONFIG,       gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_SCANCONFIG))==0) {
		hex_len = transHexScanConfig(&jobj_t, toBuff);
	} else if(strncmp(TRANS_STRING_SACNQUERY,        gjson_get_string(jobj_t.jobj_content, "type"), strlen(TRANS_STRING_SACNQUERY))==0) {
		hex_len = transHexScanQuery(&jobj_t, toBuff);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, TRANS_STRING_QUERYFIRMWARE)==0) {
		hex_len = transHexQueryFirmware(&jobj_t, toBuff);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, TRANS_STRING_WARING)==0) {
		hex_len = transHexWaring(&jobj_t, toBuff);
	} else {

	}
	//free 
	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
	if(DEBUG) {
		LOGI("transJsonToHex quits\r\n");
	};
	return hex_len;
}

static void transGetNetworkDevice(uint8_t *interface, char *rx, char *tx)
{
	uint8_t cmd[128] = {0};
	sprintf(cmd, "ubus call network.interface.%s status", interface);
	char *status = NULL;
	status = command_output_dynamic(cmd);
	if (!status) {
		LOGE("Fail to execute command: %s", cmd);
		return ;
	}
	json_object *ifstatus = json_tokener_parse(status);
	// Free the dynamic malloc memory 
	free(status);
	if (!ifstatus) {
		LOGI("%s cannot output json data", cmd);
		return ;
	}

	//Get WAN device 
	const char *dev = gjson_get_string(ifstatus, "device");
	if (!dev) {
		LOGI("%s interface not found", interface);
		json_object_put(ifstatus);
		return ;
	}
	
	char tx_cmd[128] = {0};
	char tx_bytes[32] = {0};
	char rx_cmd[128] = {0};
	char rx_bytes[32] = {0};

	sprintf(tx_cmd, "cat /sys/class/net/%s/statistics/tx_bytes", dev);
	sprintf(rx_cmd, "cat /sys/class/net/%s/statistics/rx_bytes", dev);
	command_output(tx_cmd, tx_bytes);
	command_output(rx_cmd, rx_bytes);
	memcpy(tx, tx_bytes, strlen(tx_bytes));
	memcpy(rx, rx_bytes, strlen(rx_bytes));
	
	json_object_put(ifstatus);

}

/*****************************************************************************************************
@function:      void transNetworkStatistics(uint8_t * toBuff)
@description:   Network Statistics 
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transNetworkStatistics(uint8_t * toBuff)
{
	char tx_all[32] = {0};
	char rx_all[32] = {0};
	char tx_bytes[32] = {0};
	char rx_bytes[32] = {0};
	transGetNetworkDevice("wan", rx_bytes, tx_bytes);
	double tx_wan = strtoull(tx_bytes, NULL, 10)/1024.0/1024;
	double rx_wan = strtoull(rx_bytes, NULL, 10)/1024.0/1024;
	
	transGetNetworkDevice("lan", rx_bytes, tx_bytes);
	double tx_lan = strtoull(tx_bytes, NULL, 10)/1024.0/1024;
	double rx_lan = strtoull(rx_bytes, NULL, 10)/1024.0/1024;
	
	sprintf(tx_all, "%.1lf", tx_wan+tx_lan);
	sprintf(rx_all, "%.1lf", rx_wan+rx_lan);
	
	LOGI("txb:%s==%.2lf\n", tx_all, tx_wan+tx_lan);
	LOGI("rxb:%s==%.2lf\n", rx_all, rx_wan+rx_lan);

	// Free json object
	//json_object_put(ifstatus);
	
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&jobj_t);
	
	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string("DataFlow"));
	
	json_object_object_add(jobj_t.jobj_data, "rx", json_object_new_string(rx_all));
	json_object_object_add(jobj_t.jobj_data, "tx", json_object_new_string(tx_all));

	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);

	memcpy(toBuff, json_object_to_json_string(jobj_t.jobj), strlen(json_object_to_json_string(jobj_t.jobj)));

	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}


/*****************************************************************************************************
@function:      void transWifiInfo(uint8_t * toBuff)
@description:   Generate wifi information
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
void transWifiInfo(uint8_t * toBuff)
{
	char buf[64] = {0};
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&jobj_t);

	
	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string("WifiInfo"));
	
	struct uci_context *ctx = guci2_init();

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "ssid", buf);
	json_object_object_add(jobj_t.jobj_data, "ssid", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "key", buf);
	json_object_object_add(jobj_t.jobj_data, "password", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "encryption", buf);
	if (!strcmp(buf, "none")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("3"));
	} else if (!strcmp(buf, "psk-mixed")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("2"));
	} else if (!strcmp(buf, "psk2")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("1"));
	} else if (!strcmp(buf, "psk")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("0"));
	} else {
		/* psk-mixed by default */
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("2"));
	}

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "hidden", buf);
	json_object_object_add(jobj_t.jobj_data, "wifiVisibled", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "disabled", buf);
	if (atoi(buf) == 1) {
		json_object_object_add(jobj_t.jobj_data, "wifiEnabled", json_object_new_string("0"));
	} else {
		json_object_object_add(jobj_t.jobj_data, "wifiEnabled", json_object_new_string("1"));
	}

	if (ctx) {
		guci2_free(ctx);
	}

	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);

	memcpy(toBuff, json_object_to_json_string(jobj_t.jobj), strlen(json_object_to_json_string(jobj_t.jobj)));

	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}



/*****************************************************************************************************
@function:      void transRouterInfo(uint8_t *toBuff)
@description:   Generate routing information
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transRouterInfo(uint8_t *toBuff)
{
	char buf[64] = {0};
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&jobj_t);
	
	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string("RouterInfo"));
	
	char ip[64] = {0};
	char ip_cmd[] = "wget -T 2 http://ipecho.net/plain -O - -q ; echo";
	for(int i=0; i<1; i++) {
		memset(ip, 0, sizeof(ip));
		command_output(ip_cmd, ip);
		if(strlen(ip)>6) {
			break;
		}
	}
	ip[strlen(ip)-1] = '\0';
	uint8_t mac[48] = {0};
	memset(mac, 0, sizeof(mac));
	transCenterGetLanMac(mac);
	mac[17] = '\0';
	json_object_object_add(jobj_t.jobj_data, "mac", json_object_new_string(mac));
	json_object_object_add(jobj_t.jobj_data, "ip", json_object_new_string(ip));

	struct uci_context *ctx = guci2_init();

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "ssid", buf);
	json_object_object_add(jobj_t.jobj_data, "ssid", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "key", buf);
	json_object_object_add(jobj_t.jobj_data, "password", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "encryption", buf);
	if (!strcmp(buf, "none")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("3"));
	} else if (!strcmp(buf, "psk-mixed")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("2"));
	} else if (!strcmp(buf, "psk2")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("1"));
	} else if (!strcmp(buf, "psk")) {
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("0"));
	} else {
		/* psk-mixed by default */
		json_object_object_add(jobj_t.jobj_data, "encryption", json_object_new_string("2"));
	}

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "hidden", buf);
	json_object_object_add(jobj_t.jobj_data, "wifiVisibled", json_object_new_string(buf));

	guci2_get_idx(ctx,"wireless.@wifi-iface", 0, "disabled", buf);
	if (atoi(buf) == 1) {
		json_object_object_add(jobj_t.jobj_data, "wifiEnabled", json_object_new_string("0"));
	} else {
		json_object_object_add(jobj_t.jobj_data, "wifiEnabled", json_object_new_string("1"));
	}

	guci2_get(ctx, "cositea.firmware.version", buf);
	json_object_object_add(jobj_t.jobj_data, "version", json_object_new_string(buf));

	if (ctx) {
		guci2_free(ctx);
	}

	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);

	memcpy(toBuff, json_object_to_json_string(jobj_t.jobj), strlen(json_object_to_json_string(jobj_t.jobj)));

	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}



/*****************************************************************************************************
@function:      void transRecvDownData(uint8_t *data, uint16_t data_len, sock_ctx_t *sock_ctx)
@description:  	processing data, The data is spliced and split into json data
@param:         data:		from data.  
				data_len:	from data lengt.   
				sock_ctx:	struct, Primary transfer file descriptor
@return:        none
******************************************************************************************************/

extern void transRecvDownData(uint8_t *data, uint16_t data_len, struct mosquitto *mosq)
{
	static uint8_t rec_data_buff[1024];
	static uint16_t rec_data_buff_len = 0;

	static uint8_t left = 0;
	static uint8_t right = 0;
//	if(strstr(data, "rdss")) {
//		rdssJsonDeal(data, data_len, NULL, mosq);
//		return;
//	}
	
	for(uint16_t i=0; i<data_len; i++) {
		rec_data_buff[rec_data_buff_len] = data[i];
		rec_data_buff_len++;

		switch(data[i]) {
			case '{':
				left++;
				if(left>4 || right!=0) {
					memset(rec_data_buff, 0, sizeof(rec_data_buff));
					rec_data_buff_len = 0;
					right = 0;
					left = 1;
					rec_data_buff[rec_data_buff_len] = data[i];
					rec_data_buff_len++;
				}
				break;
			case '}':
				right++;
				if(left!=4) {
					if(left==0) {
						memset(rec_data_buff, 0, rec_data_buff_len);
						left = 0;
						right = 0;
						rec_data_buff_len = 0;
					}
				} else if(left==4 && right==4) {
					//deal data
					uint8_t hex[300];
					memset(hex, 0, sizeof(hex));
					LOGI("%s", rec_data_buff);
					uint16_t len = transJsonToHex(rec_data_buff, rec_data_buff_len, hex, mosq);

					transDebug(hex, len);
						uint16_t s = writen(fd_uuart, hex, len);
					if (s < 0) {
						LOGE("Fail to send message to serial COM\n");
						return;
					}
					memset(rec_data_buff, 0, sizeof(rec_data_buff));
					rec_data_buff_len = 0;
					right = 0;
					left = 0;

				}
				break;
			default:
				if(left==0) {
					memset(rec_data_buff, 0, rec_data_buff_len);
					left = 0;
					right = 0;
					rec_data_buff_len = 0;
				}
				break;
		}
		if(rec_data_buff_len>=sizeof(rec_data_buff)) {
			memset(rec_data_buff, 0, sizeof(rec_data_buff));
			rec_data_buff_len = 0;
			right = 0;
			left = 0;
		}
	}
}




