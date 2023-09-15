#include "opkg_manage.h"
#include "gjson.h"
#include "shell.h"
#include "guci.h"
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "com.h"

#define OPKG_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define OPKG_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

#define APP_BLE             "BleHostControl"
#define APP_STP             "Stp"
#define APP_4G              "4g"
#define APP_AC              "AccessControl"
#define APP_WAN             "Wan"
#define APP_LAN             "Lan"

#define SERVER_COSITE_MQTT  "cositea_mqtt"
#define SERVER_STP          "stp"
#define SERVER_4G           "quectel"
#define SERVER_ACCESS_CTRL  "acmanage"
#define SERVER_NETWORK      "network"

#define SECTION_BLE         "@server[0]"
#define SECTION_STP         "@stp[0]"
#define SECTION_4G          "@server[0]"
#define SECTION_AC          "@server[0]"
#define SECTION_WAN         "wan"
#define SECTION_LAN         "lan"

typedef enum{
	T_STRING,
	T_INT,
	T_BOOL,
	T_ARRAY,
	T_STRING_LC,  //字符串转小写
}OPTION_TYPE;

typedef struct {
	const char *key;
	const char *option;
	OPTION_TYPE type;
	/* 可增加允许缺省参数进行检索 */
}OPTION_t;

const OPTION_t config_cositea_mqtt_list[] = {
	{"enable",   "enable",   T_BOOL},
	{"serial",   "device",   T_STRING},
	{"host",     "host",     T_STRING},
	{"port",     "port",     T_INT},
	{"userName", "username", T_STRING},
	{"password", "password", T_STRING},
	{"clientId", "id",       T_STRING},
	{"log",      "debug",    T_BOOL},
	{"extendedApplication", "automaticble", T_BOOL},
	{"extendedRssiFilter",  "filter",       T_INT},
	{"extendedlog",         "blelog",       T_BOOL},
	{"pubTopic", "pub",     T_ARRAY},
	{"subTopic", "sub",     T_ARRAY},
};

const OPTION_t config_stp_list[] = {
	{"enable",   "enable",   T_BOOL},
	{"serial",   "device",   T_STRING},
	{"host",     "host",     T_STRING},
	{"port",     "port",     T_INT},
	{"serial",   "device",   T_STRING},
	{"baud",     "baud",     T_INT},
	{"log",      "debug",    T_BOOL},
};

const OPTION_t config_ac_list[] = {
	{"enable",   "enable",   T_BOOL},
	{"host",     "host",     T_STRING},
	{"port",     "port",     T_INT},
	{"userName", "username", T_STRING},
	{"password", "password", T_STRING},
	{"clientId", "id",       T_STRING},
	{"pubTopic", "pub",      T_ARRAY},
	{"subTopic", "sub",      T_ARRAY},
	{"log",      "debug",    T_BOOL},
};

const OPTION_t config_net_static_list[] = {
	{"proto",    "proto",    T_STRING_LC},
	{"ip",       "ipaddr",   T_STRING},
	{"mask",     "netmask",  T_STRING},
	{"gateway",  "gateway",  T_STRING},
};

const OPTION_t config_net_dhcp_list[] = {
	{"proto",    "proto",    T_STRING_LC},
};


const OPTION_t config_4g_list[] = {
	{"enable",   "enable",   T_BOOL},
};

char *strlwr(char *string)
{
	int len = 0;
	len = strlen(string);
	for(int i=0; i<len; i++)
	{
		if(string[i]>='A' && string[i]<='Z')
		{
			string[i] += 'a'-'A';
		}
	}
	return string;
}

/*
@function:      bool opkgConfigApp(json_object *obj, const char *app, char *output, int *result)
@description:   根据app服应用选择配置列表, 遍历配置列表查询配置参数obj进行配置应用, 并输出配置后的参数.
@param:         obj: config param
				app: server
				output: output server config after current param json string.
				result: 0 only query, 1 execute
@return:        true, false
*/
bool opkgConfigApp(json_object *obj, const char *app, char *output, int *result)
{
	json_object *obj_param = obj;
	
	char exe_buf[512] = {0};
	char out_value[1024] = {0};
	const char *js_str;
		
	int opt_num = 0;
	char const *config;
	char const *section;
	OPTION_t const *config_list;
	
	json_object *obj_data = json_object_new_object();
	json_object *param = json_object_new_object();
	
	if(strcasecmp(app, APP_BLE)==0) {
		OPKG_LOG_INFO("cositea_mqtt");
		config = SERVER_COSITE_MQTT;
		section = SECTION_BLE;
		opt_num = sizeof(config_cositea_mqtt_list)/sizeof(config_cositea_mqtt_list[0]);
		config_list = config_cositea_mqtt_list;
		
	} else if(strcasecmp(app, APP_STP)==0) {
		OPKG_LOG_INFO("stp");
		config = SERVER_STP;
		section = SECTION_STP;
		
		opt_num = sizeof(config_stp_list)/sizeof(config_stp_list[0]);
		config_list = config_stp_list;
		
	} else if(strcasecmp(app, APP_4G)==0) {
		OPKG_LOG_INFO("4g");
		config = SERVER_4G;
		section = SECTION_4G;
		
		opt_num = sizeof(config_4g_list)/sizeof(config_4g_list[0]);
		config_list = config_4g_list;
	} else if(strcasecmp(app, APP_AC)==0) {
		OPKG_LOG_INFO("access contrl");
		section = SECTION_AC;
		config = SERVER_ACCESS_CTRL;
		
		opt_num = sizeof(config_ac_list)/sizeof(config_ac_list[0]);
		config_list = config_ac_list;
	} else if(strcasecmp(app, APP_WAN)==0) {
		OPKG_LOG_INFO("wan net");
		section = SECTION_WAN;
		config = SERVER_NETWORK;
		
		opt_num = sizeof(config_net_static_list)/sizeof(config_net_static_list[0]);
		config_list = config_net_static_list;
		
		if(obj_param) {
			const char *value = gjson_get_string(obj_param, "proto");
			if(value) {
				if(strcasecmp(value, "Dhcp")==0) {
					opt_num = sizeof(config_net_dhcp_list)/sizeof(config_net_dhcp_list[0]);
					config_list = config_net_dhcp_list;
					//
					exec_command("uci delete network.wan.ipaddr");
					exec_command("uci delete network.wan.netmask");
					exec_command("uci delete network.wan.gateway");
				} else if(strcasecmp(value, "Static")==0) {
					opt_num = sizeof(config_net_static_list)/sizeof(config_net_static_list[0]);
					config_list = config_net_static_list;
				} else {
					return false;
				}
			} else {
				return false;
			}
		}
	} else if(strcasecmp(app, APP_LAN)==0) {
		OPKG_LOG_INFO("lan net");
		section = SECTION_LAN;
		config = SERVER_NETWORK;
		
		opt_num = sizeof(config_net_static_list)/sizeof(config_net_static_list[0]);
		config_list = config_net_static_list;
		
		if(obj_param) {
			const char *value = gjson_get_string(obj_param, "proto");
			if(value) {
				if(strcasecmp(value, "Dhcp")==0) {
					opt_num = sizeof(config_net_dhcp_list)/sizeof(config_net_dhcp_list[0]);
					config_list = config_net_dhcp_list;
					//
					exec_command("uci delete network.lan.ipaddr");
					exec_command("uci delete network.lan.netmask");
				} else if(strcasecmp(value, "Static")==0) {
					opt_num = sizeof(config_net_static_list)/sizeof(config_net_static_list[0]);
					config_list = config_net_static_list;
				} else {
					return false;
				}
			} else {
				return false;
			}
		}
	} else {
		//not support app
		json_object_put(param);
		json_object_put(obj_data);
		return false;
	}
	
	*result = 0;
	if(obj_param == NULL) 
		goto only_query;
	
	//uci set <config>.<section>[.<option>]=<value>
	*result = 1;
	
	for(int i=0; i<opt_num; i++) {
		if(config_list[i].type == T_STRING) {
			const char *value = gjson_get_string(obj_param, config_list[i].key);
			if(value) {
				memset(exe_buf, 0 ,sizeof(exe_buf));
				sprintf(exe_buf, "uci set %s.%s.%s=\"%s\"", config, section, config_list[i].option, value);
				OPKG_LOG_INFO("cmd:%s", exe_buf);
				exec_command(exe_buf);
			}
		} else if(config_list[i].type == T_STRING_LC) {
			const char *value = gjson_get_string(obj_param, config_list[i].key);
			if(value) {
				char buf[128] = {0};
				strcpy(buf, value);
				strlwr(buf);
				memset(exe_buf, 0 ,sizeof(exe_buf));
				sprintf(exe_buf, "uci set %s.%s.%s=\"%s\"", config, section, config_list[i].option, buf);
				OPKG_LOG_INFO("cmd:%s", exe_buf);
				exec_command(exe_buf);
			}
		} else if(config_list[i].type == T_INT) {
			int value = gjson_get_int(obj_param, config_list[i].key);
			/*when json value is -1, this error*/
			if(value != -1) {
				memset(exe_buf, 0 ,sizeof(exe_buf));
				sprintf(exe_buf, "uci set %s.%s.%s=\"%d\"", config, section, config_list[i].option, value);
				OPKG_LOG_INFO("cmd:%s", exe_buf);
				exec_command(exe_buf);
			}
		} else if(config_list[i].type == T_BOOL) {
			int res = 0;
			const char *value = gjson_get_string(obj_param, config_list[i].key);
			if(value) {
				if(strcasecmp(value, "enable")==0 ||
				   strcasecmp(value, "true")==0 ||
				   strcasecmp(value, "1")==0) {
					res = 1;   
				} else if(strcasecmp(value, "disable")==0 ||
				   strcasecmp(value, "false")==0 ||
				   strcasecmp(value, "0")==0) {
					res = 0;   
				}
				memset(exe_buf, 0 ,sizeof(exe_buf));
				sprintf(exe_buf, "uci set %s.%s.%s=\"%d\"", config, section, config_list[i].option, res);
				OPKG_LOG_INFO("cmd:%s", exe_buf);
				exec_command(exe_buf);
			} else {
				res = gjson_get_int(obj_param, config_list[i].key);
				/*when json value is -1, this error*/
				if(res != -1) {
					memset(exe_buf, 0 ,sizeof(exe_buf));
					sprintf(exe_buf, "uci set %s.%s.%s=\"%d\"", config, section, config_list[i].option, res?1:0);
					OPKG_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
				}
			}
		} else if(config_list[i].type == T_ARRAY) {
			array_list *array = gjson_get_array(obj_param, config_list[i].key);
			
			if(array) {
				memset(exe_buf, 0 ,sizeof(exe_buf));
				sprintf(exe_buf, "uci delete %s.%s.%s", config, section, config_list[i].option);
				OPKG_LOG_INFO("cmd:%s", exe_buf);
				exec_command(exe_buf);
				
				for(int j=0; j<array->length; j++) {
					const char *node = json_object_to_json_string(array_list_get_idx(array, j));
					memset(exe_buf, 0 ,sizeof(exe_buf));
					char filter_buf[128] = {0};
					int filter_length = 0;
					for(int k=0; k<strlen(node); k++) {
						if(node[k] != '\\' && node[k] != '"') {
							filter_buf[filter_length++] = node[k];
						}
					}
					filter_buf[filter_length++] = '\0';
					sprintf(exe_buf, "uci add_list %s.%s.%s=\"%s\"", config, section, config_list[i].option, filter_buf);
					OPKG_LOG_INFO("cmd:%s", exe_buf);
					exec_command(exe_buf);
				}
			}
		}
	}
	
	memset(exe_buf, 0 ,sizeof(exe_buf));
	sprintf(exe_buf, "uci commit %s", config);
	OPKG_LOG_INFO("cmd:%s", exe_buf);
	exec_command(exe_buf);
	memset(exe_buf, 0 ,sizeof(exe_buf));
	sprintf(exe_buf, "/etc/init.d/%s restart", config);
	OPKG_LOG_INFO("cmd:%s", exe_buf);
	exec_command(exe_buf);
	if(strcmp(config, SERVER_NETWORK) == 0) {
		//配置网络后socket失效, 需要重连, 未实现, 直接重启服务.
		//opkgServiceRestart(SERVER_ACCESS_CTRL);
		exec_command("reboot");
	}
	
	only_query:
	
	for(int i=0; i<opt_num; i++) {
		memset(exe_buf, 0 ,sizeof(exe_buf));
		memset(out_value, 0, sizeof(out_value));
		sprintf(exe_buf, "uci get %s.%s.%s", config, section, config_list[i].option);
		OPKG_LOG_INFO("cmd:%s", exe_buf);
		command_output(exe_buf, out_value);
		//delete last byte '\n'
		if(strlen(out_value))
			out_value[strlen(out_value)-1] = '\0';
		if(config_list[i].type == T_ARRAY) {
			
			struct json_object *array = json_object_new_array();
			char *topic=strtok(out_value," ");
			while(topic) {
				json_object_array_add(array, json_object_new_string(topic));
				topic=strtok(NULL," ");
			}
			json_object_object_add(param, config_list[i].key, array);
		} else if(config_list[i].type == T_BOOL) {
			if(out_value[0] == '0') {
				json_object_object_add(param, config_list[i].key, json_object_new_string("Disable"));
			} else {
				json_object_object_add(param, config_list[i].key, json_object_new_string("Enable"));
			}
		} else {
			json_object_object_add(param, config_list[i].key, json_object_new_string(out_value));
		}
	}
	json_object_object_add(obj_data, "service", json_object_new_string(app));

	json_object_object_add(obj_data, "param", param);
	
	js_str = json_object_to_json_string(obj_data);
	
	OPKG_LOG_INFO("output json length %d", strlen(js_str));
	memcpy(output, js_str, strlen(js_str));
	
	json_object_put(param);
	json_object_put(obj_data);
	
	return true;
}

void opkgServiceRestart(const char *service)
{
	char exe_buf[512] = {0};
	memset(exe_buf, 0 ,sizeof(exe_buf));
	sprintf(exe_buf, "/etc/init.d/%s restart", service);
	
	OPKG_LOG_INFO("restart server %s!", service)
	OPKG_LOG_INFO("cmd:%s", exe_buf);
	exec_command(exe_buf);
}










