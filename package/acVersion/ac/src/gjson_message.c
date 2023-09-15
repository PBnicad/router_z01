#include "gjson_message.h"
#include "utils.h"
#include "file_download.h"
#include "mqtt_callbacks.h"
#include "udp_network.h"

#include "com.h"
#include <string.h>

#define JSON_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define JSON_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

#define ROUTER_UPGRADE_PATH "/tmp/upgrade.bin"

#include "gjson.h"
#include "guci.h"
#include "shell.h"
#include "com.h"
#include "router_upgrader.h"
#include "opkg_manage.h"
#include "ble_upgrade.h"

//object string
#define	TRANS_JOBJ_COMTYPE	"comType"
#define	TRANS_JOBJ_CONTENT	"content"
#define	TRANS_JOBJ_DATA		"data"

#define jsonStrTypeCmp(json_obj, str) strncmp(str, gjson_get_string(json_obj, "type"), strlen(str))

#define	KEY_STRING_SYSUPGRADE			"RouterUpgrade"
#define KEY_STRING_PACKAGEUPGRADE       "PackageUpgrade"
#define KEY_STRING_SERVICECONFIG        "ServiceConfig"
//#define KEY_STRING_NETWORKCONFIG      "RouterNetworkConfig"
#define KEY_STRING_MACCONFIG            "RouterMacConfig"
#define KEY_STRING_PING                 "PingTest"
#define KEY_STRING_HOSTBLEUPGRADE       "BleUpgrade"
#define KEY_STRING_AREANET              "AreaNet"
#define KEY_STRING_REBOOT               "Reboot"
#define KEY_STRING_ROUTERINFO           "RouterInfo"

//one json pack
typedef struct {
	json_object *jobj;
	json_object *jobj_comType;
	json_object *jobj_content;
	json_object *jobj_data;
}TRANS_JOBJ_t;


#define MESSAGE_MAX_SIZE 2048



void gjsonMessageReply(unsigned char *message, unsigned int length, MESSAGE_TYPE msg_type)
{
	if(msg_type==MESSAGE_UDP) {
		udpSocketSendMessage(message, length);
	} else if(msg_type==MESSAGE_MQTT){
		mqttMessagePublish((char *)message);
	}
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
	json_object_object_add(jobj_t->jobj_comType, "time", json_object_new_string(buf));

	uint8_t clientId[24];
	memset(clientId, 0, sizeof(clientId));
	
	memcpy(clientId, get_mac(true), 12);

	json_object_object_add(jobj_t->jobj_comType, "clientMac", json_object_new_string(clientId));
}

/*****************************************************************************************************
@function:      void transRouterInfo(uint8_t *toBuff)
@description:   Generate routing information
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transRouterInfo(uint8_t *toBuff)
{
	char buf[128] = {0};
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

	//guci2_get(ctx, "cositea.firmware.version", buf);
	build_time_get(buf);
	json_object_object_add(jobj_t.jobj_data, "version", json_object_new_string(buf));
	
	FILE *file = fopen("/ble_version", "r");
	if(file) {
		char version[60] = {0};
		fread(version, sizeof(version), 1, file);
		fclose(file);
		json_object_object_add(jobj_t.jobj_data, "bleVersion", json_object_new_string(version));
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

static void gjsonUpgradeError(MESSAGE_TYPE msg_type, UPGRADER_EVENT status)
{
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&jobj_t);

	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_SYSUPGRADE));
	
	if(status == ROUTER_UPGRADER_404)
	{
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("404"));
	}
	else
	{
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("error"));
	}
	
	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);	

	const char *js_str = json_object_to_json_string(jobj_t.jobj);
	JSON_LOG_INFO("gjsonUpgradeError %s", js_str);
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
	
	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}

void gjsonUpgradeBleCallback(uint8_t *res_code, MESSAGE_TYPE msg_type)
{
	TRANS_JOBJ_t jobj_t;
	jobj_t.jobj = json_object_new_object();
	jobj_t.jobj_comType = json_object_new_object();
	jobj_t.jobj_content = json_object_new_object();
	jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&jobj_t);

	json_object_object_add(jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_HOSTBLEUPGRADE));
	
	if(res_code[0]==OTHER_ERR_TIMEOUT) {
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("Timeout"));
	} else if(res_code[0]==OTHER_ERR_SUCCESS){
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("Success"));
	} else if(res_code[0]==OTHER_ERR_404){
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("404"));
	} else if(res_code[0]==OTHER_ERR_BUSY){
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string("Busy"));
	} else if(res_code[0]==0x0B){
		char buf[40] = {0};
		snprintf(buf, sizeof(buf), "Fail,0x%02X,0x%02X", res_code[0], res_code[1]);
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string(buf));
	} else {
		char buf[40] = {0};
		snprintf(buf, sizeof(buf), "Fail,0x%02X", res_code[0]);
		json_object_object_add(jobj_t.jobj_data, "update", json_object_new_string(buf));
	}
	//蓝牙主机版本查询是在cositea_mqtt内首次运行进行一次自动查询,每次升级操作结束都重启一遍
	opkgServiceRestart("cositea_mqtt");
	
	sleep(3);
	
	JSON_LOG_INFO("get ble version. %s, %s", __DATE__, __TIME__);
	FILE *file = fopen("/ble_version", "r");
	if(file) {
		char version[60] = {0};
		fread(version, sizeof(version), 1, file);
		fclose(file);
		json_object_object_add(jobj_t.jobj_data, "version", json_object_new_string(version));
	}
	
	json_object_object_add(jobj_t.jobj, TRANS_JOBJ_COMTYPE, jobj_t.jobj_comType);
	json_object_object_add(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, jobj_t.jobj_content);
	json_object_object_add(jobj_t.jobj_content, TRANS_JOBJ_DATA, jobj_t.jobj_data);	
	
	const char *js_str = json_object_to_json_string(jobj_t.jobj);
	JSON_LOG_INFO("gjsonUpgrade404 %s", js_str);
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
	
	json_object_put(jobj_t.jobj_data);
	json_object_put(jobj_t.jobj_content);
	json_object_put(jobj_t.jobj_comType);
	json_object_put(jobj_t.jobj);
}

void gjsonDownloadFileCallbacks(uint32_t id, int status, char *path, MESSAGE_TYPE msg_type)
{
	if(strstr(path, ".ipk")) {
		if(status == 0)
		{
			//download success, upgrade
			JSON_LOG_INFO("get ipk package, will be upgrade");
		}
	}
	if(strstr(path, ".zip")) {
		if(status == 0) {
			//download success, ble upgrade
			JSON_LOG_INFO("get zip package, will be upgrade\n");
			upgradeBle(path, msg_type, gjsonUpgradeBleCallback);
		}
		else {
			uint8_t err[2] = {0xf1,0};
			gjsonUpgradeBleCallback(err, msg_type);
		}
	}
}

/*****************************************************************************************************
@function:      static uint16_t gjsonUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
@description:   update routeing firmware
@param:         jobj_t:		json object;
@return:        0, zero
******************************************************************************************************/
static void gjsonUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	int rc = 0;
	
	const char *url = gjson_get_string(jobj_t->jobj_data, "url");
	int keepSetting = gjson_get_int(jobj_t->jobj_data, "keepSetting");

	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();

	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_SYSUPGRADE));
	
	bool ret = routerUpgradePthreadCreate((char *)url, keepSetting, msg_type);
	routerUpgradeFailCallbackSet(gjsonUpgradeError);
	if(ret)
	{
		JSON_LOG_INFO("gjsonUpgrade: ReadyUpdate");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("ReadyUpdate"));
	}
	else
	{
		JSON_LOG_INFO("gjsonUpgrade: Fail");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("Fail"));
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	JSON_LOG_INFO("gjsonUpgrade %s", js_str);
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
						
	json_object_put(rsp_jobj_t.jobj);
}


/*****************************************************************************************************
@function:      static uint16_t gjsonBleUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
@description:   update routeing firmware
@param:         jobj_t:		json object;
@return:        0, zero
******************************************************************************************************/
static void gjsonBleUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	fileDownloadCallbackSet(gjsonDownloadFileCallbacks);
	
	const char *url = gjson_get_string(jobj_t->jobj_data, "url");

	
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_HOSTBLEUPGRADE));
	
	bool ret = fileDownloadTaskStart((char *)url, "/tmp/data.zip", msg_type);
	if(ret)
	{
		JSON_LOG_INFO("gjsonBleUpgrade: ReadyUpdate");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("ReadyUpdate"));
	}
	else
	{
		JSON_LOG_INFO("gjsonPackageUpgrade: Fail");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("Fail"));
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
	
	json_object_put(rsp_jobj_t.jobj);
}

/*****************************************************************************************************
@function:      static uint16_t gjsonPackageUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
@description:   update routeing firmware
@param:         jobj_t:		json object;
@return:        0, zero
******************************************************************************************************/
static void gjsonPackageUpgrade(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	fileDownloadCallbackSet(gjsonDownloadFileCallbacks);
	
	const char *url = gjson_get_string(jobj_t->jobj_data, "url");

	
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_PACKAGEUPGRADE));
	
	bool ret = fileDownloadTaskStart((char *)url, "/tmp/data.ipk", msg_type);
	if(ret)
	{
		JSON_LOG_INFO("gjsonPackageUpgrade: ReadyUpdate");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("ReadyUpdate"));
	}
	else
	{
		JSON_LOG_INFO("gjsonPackageUpgrade: Fail");
		json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("Fail"));
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
						
	json_object_put(rsp_jobj_t.jobj);

}

void gjsonServiceConfig(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	const char *configApp = gjson_get_string(jobj_t->jobj_data, "service");
	int result = 0;
	
	json_object *param_obj = gjson_get_object(jobj_t->jobj_data, "param");
	
	if(configApp==NULL) {
		JSON_LOG_INFO("param error");
		return;
	}
	
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	//rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_SERVICECONFIG));
	
	char js_buf[512] = {0};
	bool res = opkgConfigApp(param_obj, configApp, js_buf, &result);
	if(res) {
		gjson_string_to_json(js_buf, strlen(js_buf), &rsp_jobj_t.jobj_data);
		if(result) {
			json_object_object_add(rsp_jobj_t.jobj_data, "result", json_object_new_string("Success"));
		} else {
			json_object_object_add(rsp_jobj_t.jobj_data, "result", json_object_new_string("Query"));
		}
	} else {
		rsp_jobj_t.jobj_data = json_object_new_object();
		json_object_object_add(rsp_jobj_t.jobj_data, "service", json_object_new_string(configApp));
		json_object_object_add(rsp_jobj_t.jobj_data, "status", json_object_new_string("NotFound"));
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	char filter_js_str[1024] = {0};
	int filter_length = 0;
	for(int k=0; k<strlen(js_str); k++) {
		if(js_str[k] == '\\') {
			//filter
		} else {
			filter_js_str[filter_length++] = js_str[k];
		}
	}
	filter_js_str[filter_length++] = '\0';
	
	gjsonMessageReply(filter_js_str, strlen((char *)filter_js_str), msg_type);
						
	json_object_put(rsp_jobj_t.jobj);
}

void gjsonGetShareInfo(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_AREANET));
	
	struct json_object *array;
	char mac_str[20] = {0};
	unsigned char mac[6] = {0};
	char info[256] = {0};
	char *ipstr = NULL;
	bool loop=gwShareInfoViewTok(true, mac, &ipstr);
	while(loop) {
		array = json_object_new_array();
		
		memset(mac_str, 0, sizeof(mac_str));
		snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		
		JSON_LOG_INFO("view info %s:%s\n", mac_str, ipstr);
		memset(info, 0, sizeof(info));
		stpcpy(info, ipstr);
		char *ip = strtok(info, ",");
		while(ip) {
			json_object_array_add(array, json_object_new_string(ip));
			ip = strtok(NULL, ",");
		}
		
		json_object_object_add(rsp_jobj_t.jobj_data, mac_str, array);
		
		memset(mac, 0, sizeof(mac));
		loop = gwShareInfoViewTok(false, mac, &ipstr);
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
						
	json_object_put(rsp_jobj_t.jobj);
}

void gjsonRouterReboot(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_REBOOT));
	
	json_object_object_add(rsp_jobj_t.jobj_data, "response", json_object_new_string("Ok"));
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
						
	json_object_put(rsp_jobj_t.jobj);
	
	sleep(1);
	do_reboot();
}

void gjsonRouterInfo(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	uint8_t buf_router_info[512];
	memset(buf_router_info, 0, sizeof(buf_router_info));
	transRouterInfo(buf_router_info);

	gjsonMessageReply((char *)buf_router_info, strlen((char *)buf_router_info), msg_type);
}
/*
void gjsonNetworkConfig(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_NETWORKCONFIG));
	
	const char *interface = gjson_get_string(jobj_t->jobj_data, "interface");
	const char *proto = gjson_get_string(jobj_t->jobj_data, "proto");
	const char *ipaddr = gjson_get_string(jobj_t->jobj_data, "ipaddr");
	const char *netmask = gjson_get_string(jobj_t->jobj_data, "netmask");
	
	
	char exe_buf[512] = {0};
	if(interface==NULL || interface==NULL) {
		
	} else {
		if(strcmp(interface, "wan")==0) {
			if(strcmp(proto, "static")==0) {
				if(ipaddr!=NULL && netmask!=NULL) {
					JSON_LOG_INFO("cmd:%s", "uci set network.wan.proto=static");
					exec_command("uci set network.wan.proto=static");
					memset(exe_buf, 0 ,sizeof(exe_buf));
					sprintf(exe_buf, "uci set network.wan.ipaddr=%s", ipaddr);
					JSON_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
					memset(exe_buf, 0 ,sizeof(exe_buf));
					sprintf(exe_buf, "uci set network.wan.netmask=%s", netmask);
					JSON_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
				}
			} else if(strcmp(proto, "dhcp")==0) {
				JSON_LOG_INFO("cmd:%s", "uci set network.wan.proto=dhcp");
				exec_command("uci set network.wan.proto=dhcp");
			}
		} else if(strcmp(interface, "lan")==0) {
			if(strcmp(proto, "static")==0) {
				if(ipaddr!=NULL && netmask!=NULL) {
					JSON_LOG_INFO("cmd:%s", "uci set network.lan.proto=static");
					exec_command("uci set network.lan.proto=static");
					memset(exe_buf, 0 ,sizeof(exe_buf));
					sprintf(exe_buf, "uci set network.lan.ipaddr=%s", ipaddr);
					JSON_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
					memset(exe_buf, 0 ,sizeof(exe_buf));
					sprintf(exe_buf, "uci set network.lan.netmask=%s", netmask);
					JSON_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
				}
			}
		}
	}
	
	
	char out_value[200] = {0};
	
	memset(exe_buf, 0 ,sizeof(exe_buf));
	sprintf(exe_buf, "uci get network.%s.proto", interface);
	JSON_LOG_INFO("cmd:%s", exe_buf);
	command_output(exe_buf, out_value);
	json_object_object_add(rsp_jobj_t.jobj_data, "proto", json_object_new_string(out_value));
		
	if(strcmp(proto, "static")==0) {
		memset(exe_buf, 0 ,sizeof(exe_buf));
		sprintf(exe_buf, "uci get network.%s.proto", interface);
		JSON_LOG_INFO("cmd:%s", exe_buf);
		command_output(exe_buf, out_value);
		json_object_object_add(rsp_jobj_t.jobj_data, "proto", json_object_new_string(out_value));
		
		memset(exe_buf, 0 ,sizeof(exe_buf));
		sprintf(exe_buf, "uci get network.%s.netmask", interface);
		JSON_LOG_INFO("cmd:%s", exe_buf);
		command_output(exe_buf, out_value);
		json_object_object_add(rsp_jobj_t.jobj_data, "netmask", json_object_new_string(out_value));
	}
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
	
	json_object_put(rsp_jobj_t.jobj);
}*/

void gjsonRouterMacConfig(TRANS_JOBJ_t *jobj_t, MESSAGE_TYPE msg_type)
{
	const char *mac = gjson_get_string(jobj_t->jobj_data, "mac");
	const char *old_mac = gjson_get_string(jobj_t->jobj_data, "oldmac");
	
	char filter_old_mac[100] = {0};
	int filter_length = 0;
	if(old_mac && mac) {
		for(int i=0; i<strlen(old_mac); i++) {
			if(old_mac[i] != ':') {
				filter_old_mac[filter_length++] = old_mac[i];
			}
		}
		
		if(strcasecmp(get_mac(true), filter_old_mac)==0) {
			char exe_buf[512] = {0};
			memset(exe_buf, 0 ,sizeof(exe_buf));
			sprintf(exe_buf, "setmac %s", mac);
			JSON_LOG_INFO("cmd:%s", exe_buf);
			exec_command(exe_buf);
		}
	}
	
	//if config success, system will be reboot, can't come to here
	
	TRANS_JOBJ_t rsp_jobj_t;
	rsp_jobj_t.jobj = json_object_new_object();
	rsp_jobj_t.jobj_comType = json_object_new_object();
	rsp_jobj_t.jobj_content = json_object_new_object();
	rsp_jobj_t.jobj_data = json_object_new_object();
	
	transJsonHearder(&rsp_jobj_t);
	
	json_object_object_add(rsp_jobj_t.jobj_content, "type", json_object_new_string(KEY_STRING_MACCONFIG));
	json_object_object_add(rsp_jobj_t.jobj_data, "status", json_object_new_string("error"));
	
	json_object_object_add(rsp_jobj_t.jobj, TRANS_JOBJ_COMTYPE, rsp_jobj_t.jobj_comType);
	json_object_object_add(rsp_jobj_t.jobj_comType, TRANS_JOBJ_CONTENT, rsp_jobj_t.jobj_content);
	json_object_object_add(rsp_jobj_t.jobj_content, TRANS_JOBJ_DATA, rsp_jobj_t.jobj_data);
	
	const char *js_str = json_object_to_json_string(rsp_jobj_t.jobj);
	
	gjsonMessageReply((char *)js_str, strlen((char *)js_str), msg_type);
	
	json_object_put(rsp_jobj_t.jobj);
}

void gjsonMessageCallback(uint8_t *json_buf, uint16_t json_buff_len, MESSAGE_TYPE msg_type)
{
	TRANS_JOBJ_t jobj_t;
	if(gjson_string_to_json(json_buf, json_buff_len, &jobj_t.jobj)==0) {
		//JSON_LOG_INFO("string to json success\n");
		if(jobj_t.jobj == NULL) {
			JSON_LOG_INFO("convert  null object");
			return;
		}
	} else {
		JSON_LOG_INFO("convert  string to json [fail]");
		return;
	}
	jobj_t.jobj_comType = gjson_get_object(jobj_t.jobj, TRANS_JOBJ_COMTYPE);
	if(jsonStrTypeCmp(jobj_t.jobj_comType, "Up")==0) {
		JSON_LOG_INFO("error message drop.");
		json_object_put(jobj_t.jobj_comType);
		json_object_put(jobj_t.jobj);
		return;
	} 
	jobj_t.jobj_content = gjson_get_object(jobj_t.jobj_comType, TRANS_JOBJ_CONTENT);
	jobj_t.jobj_data = gjson_get_object(jobj_t.jobj_content, TRANS_JOBJ_DATA);
	
	if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_SYSUPGRADE)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_SYSUPGRADE);
		gjsonUpgrade(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_PACKAGEUPGRADE)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_PACKAGEUPGRADE);
		gjsonPackageUpgrade(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_SERVICECONFIG)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_SERVICECONFIG);
		gjsonServiceConfig(&jobj_t, msg_type);
//	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_NETWORKCONFIG)==0) {
//		JSON_LOG_INFO("parse: %s", KEY_STRING_NETWORKCONFIG);
//		gjsonNetworkConfig(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_MACCONFIG)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_MACCONFIG);
		gjsonRouterMacConfig(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_PING)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_PING);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_HOSTBLEUPGRADE)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_HOSTBLEUPGRADE);
		gjsonBleUpgrade(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_AREANET)==0) {
		JSON_LOG_INFO("get share info: %s", KEY_STRING_AREANET);
		gjsonGetShareInfo(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_REBOOT)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_REBOOT);
		gjsonRouterReboot(&jobj_t, msg_type);
	} else if(jsonStrTypeCmp(jobj_t.jobj_content, KEY_STRING_ROUTERINFO)==0) {
		JSON_LOG_INFO("parse: %s", KEY_STRING_ROUTERINFO);
		gjsonRouterInfo(&jobj_t, msg_type);
	}
	//free 
	json_object_put(jobj_t.jobj);
	
}





