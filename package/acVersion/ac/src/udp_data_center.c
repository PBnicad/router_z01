#include "udp_data_center.h"

#include "shell.h"
#include "utils.h"
#include "com.h"
#include "udp_network.h"
#include "gjson_message.h"
#include <libubox/uloop.h>
#include "gjson.h"



#define CEN_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define CEN_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

#define SHAREINFO_TIMEOUT_PERIOD 6

typedef struct{
	unsigned char mac[6];
	char *ipstr;
	unsigned short timeout;
	void *next;
}GW_SHARE_INFO_t;

GW_SHARE_INFO_t *gw_share_info = NULL;


void updOutputInfoToFile(const char *filename, const char *json_string)
{
	FILE *file = NULL;
	
	file = fopen(filename, "w");
	if(file) {
		fwrite(json_string, 1, strlen(json_string), file);
		fclose(file);
	}
}

void udpShareInfoSave(void)
{
	json_object *obj = json_object_new_object();
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
		
		//JSON_LOG_INFO("view info %s:%s\n", mac_str, ipstr);
		memset(info, 0, sizeof(info));
		strcpy(info, ipstr);
		char *ip = strtok(info, ",");
		while(ip) {
			json_object_array_add(array, json_object_new_string(ip));
			ip = strtok(NULL, ",");
		}
		
		json_object_object_add(obj, mac_str, array);
		
		memset(mac, 0, sizeof(mac));
		loop = gwShareInfoViewTok(false, mac, &ipstr);
	}
	const char *js_str = json_object_to_json_string(obj);
	updOutputInfoToFile("/tmp/shareInfo", js_str);
	
	json_object_put(obj);
}

void gwShareAppend(unsigned char *mac, char *ip)
{
	GW_SHARE_INFO_t *info = (GW_SHARE_INFO_t *)ss_malloc(sizeof(GW_SHARE_INFO_t));
	memset(info, 0, sizeof(GW_SHARE_INFO_t));
	char *string_ip = (char *)ss_malloc(strlen(ip)+1);
	memset(string_ip, 0, strlen(ip)+1);
	
	memcpy(info->mac, mac, 6);
	memcpy(string_ip, ip, strlen(ip));
	info->ipstr = string_ip;
	info->next = NULL;
	
	if(gw_share_info)
	{
		GW_SHARE_INFO_t *poll = gw_share_info;
		while(poll->next != NULL)
		{
			poll = (GW_SHARE_INFO_t *)poll->next;
		}
		poll->next = info;
	}
	else
	{
		gw_share_info = info;
	}
	udpShareInfoSave();
}

bool gwShareInfoUpdate(unsigned char *mac, char *ip)
{
	GW_SHARE_INFO_t *poll = gw_share_info;
	while(poll)
	{
		if(memcmp(poll->mac, mac, 6)==0)
		{
			if(strcmp(poll->ipstr, ip)!=0) {
				CEN_LOG_INFO("ip changed %s->%s", poll->ipstr, ip);
				char *string_ip = (char *)ss_malloc(strlen(ip)+1);
				memset(string_ip, 0, strlen(ip)+1);
				
				memcpy(string_ip, ip, strlen(ip));
				
				ss_free(poll->ipstr);
				poll->ipstr = string_ip;
				
				udpShareInfoSave();
			}
			poll->timeout = 0;
			
			return true;
		}
		poll = poll->next;
	}
	return false;
}

void gwShareInfoTimeoutUpdate(void)
{
	bool changed = false;
	GW_SHARE_INFO_t *poll = gw_share_info;
	GW_SHARE_INFO_t *last_poll = NULL;
	
	while(poll)
	{
		poll->timeout++;
		
		if(poll->timeout > SHAREINFO_TIMEOUT_PERIOD) {
			GW_SHARE_INFO_t *fr_node = poll;
			if(fr_node==gw_share_info) {
				//delete head
				CEN_LOG_INFO("delete head share info %s", fr_node->ipstr);
				gw_share_info = fr_node->next;
			} else {
				CEN_LOG_INFO("delete share info %s", fr_node->ipstr);
				last_poll->next = fr_node->next;
				//go back
				poll = last_poll; //
			}
			ss_free(fr_node->ipstr);
			ss_free(fr_node);
			changed = true;
		}
		
		last_poll = poll;
		poll = poll->next;
	}
	if(changed)
		udpShareInfoSave();
}

bool gwShareInfoViewTok(bool first, unsigned char *mac, char **ipStr)
{
	static GW_SHARE_INFO_t *poll = NULL;
	if(first) {
		poll = gw_share_info;
	} else {
		poll = poll->next;
	}
	if(poll) {
		memcpy(mac, poll->mac, 6);
		*ipStr = poll->ipstr;
			
		return true;
	}
	return false;
}


void udpMessageHeartbeat(void)
{
	const unsigned char *hex_mac = udpGetMac();
	const unsigned char *hex_ip = udpGetIp();
	const char *all_ip = updGetAllIpString();
	
	json_object *obj = json_object_new_object();
	
	char mac[24] = {0};
	//char ip[20] = {0};
	snprintf(mac, sizeof(mac), "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", hex_mac[0], hex_mac[1], hex_mac[2], hex_mac[3], hex_mac[4], hex_mac[5]);
	//snprintf(ip, sizeof(ip), "%d.%d.%d.%d", hex_ip[0], hex_ip[1], hex_ip[2], hex_ip[3]);
	
	json_object_object_add(obj, "mac", json_object_new_string(mac));
	json_object_object_add(obj, "ip", json_object_new_string(all_ip));
	
	const char *js_str = json_object_to_json_string(obj);
	
	udpSocketSendMessage((unsigned char *)js_str, strlen(js_str));
						
	json_object_put(obj);
	
	gwShareInfoTimeoutUpdate();
}

void udpMessageHandler(unsigned char *data, uint32_t len)
{
	json_object *obj;
	if(gjson_string_to_json(data, len, &obj)==0) {
		//CEN_LOG_INFO("udp string to json success\n");
	} else {
		CEN_LOG_INFO("udp convert string to json [fail]");
		return;
	}
	json_object *jobj_comType = gjson_get_object(obj, "comType");
	
	if(jobj_comType) {
		if(strcmp("Up", gjson_get_string(jobj_comType, "type"))==0) {
			//CEN_LOG_INFO("udp message drop.");
			json_object_put(jobj_comType);
			json_object_put(obj);
			return;
		} else {
			const char *comMac = gjson_get_string(obj, "comMac");
			if(comMac == NULL || (strcasecmp(comMac, get_mac(true))!=0 && strlen(comMac)!=0)) {
				return;
			}
		}
	}
	CEN_LOG_INFO("udp receive message:%s\n", data);
	
	const char *mac = gjson_get_string(obj, "mac");
	if(mac) {
		const char *ip = gjson_get_string(obj, "ip");
		unsigned char hex_mac[6] = {0};
		//unsigned char hex_ip[4] = {0};
		
		sscanf(mac, "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX", &hex_mac[0], &hex_mac[1], &hex_mac[2], &hex_mac[3], &hex_mac[4], &hex_mac[5]);
		//sscanf(ip, "%hhd.%hhd.%hhd.%hhd", &hex_ip[0], &hex_ip[1], &hex_ip[2], &hex_ip[3]);
		
		CEN_LOG_INFO("%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", hex_mac[0], hex_mac[1], hex_mac[2], hex_mac[3], hex_mac[4], hex_mac[5]);
		//CEN_LOG_INFO("%d.%d.%d.%d", hex_ip[0], hex_ip[1], hex_ip[2], hex_ip[3]);
		
		if(!gwShareInfoUpdate(hex_mac, (char *)ip)) {
			gwShareAppend(hex_mac, (char *)ip);
		}
	} else {
		gjsonMessageCallback(data, len, MESSAGE_UDP);
	}
	json_object_put(jobj_comType);
	json_object_put(obj);
}


