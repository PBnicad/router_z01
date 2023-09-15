#include "mqtt_callbacks.h"
#include "gjson_message.h"
#include <libubox/list.h>
#include <libubox/utils.h>
#include "com.h"
#include "utils.h"
#include "gjson.h"
#include "mqtt_topic.h"

#define MQTT_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define MQTT_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

/***************************  ****************************/

void mqttReturnCheck(int ret, char *info)
{
	switch(ret) {
		case MOSQ_ERR_SUCCESS:
			MQTT_LOG_INFO("MOSQ_ERR_SUCCESS: %s", info);
			break;
		case MOSQ_ERR_NOMEM:
			MQTT_LOG_ERROR("Publish 内存不足: %s", info);
			break;
		case MOSQ_ERR_PROTOCOL:
			MQTT_LOG_ERROR("Publish 协议错误: %s", info);
			break;
		case MOSQ_ERR_INVAL:
			MQTT_LOG_ERROR("Publish 参数错误: %s", info);
			break;
		case MOSQ_ERR_NO_CONN:
			MQTT_LOG_ERROR("Publish broker连接未建立: %s", info);
			break;
		case MOSQ_ERR_PAYLOAD_SIZE:
			MQTT_LOG_ERROR("Publish 数据过大: %s", info);
			break;
		case MOSQ_ERR_MALFORMED_UTF8:
			MQTT_LOG_ERROR("Publish 主题编码是无效的UTF8: %s", info);

			break;
		case MOSQ_ERR_CONN_PENDING:
		case MOSQ_ERR_CONN_REFUSED:
		case MOSQ_ERR_NOT_FOUND:
		case MOSQ_ERR_CONN_LOST:
		case MOSQ_ERR_TLS:
		case MOSQ_ERR_NOT_SUPPORTED:
		case MOSQ_ERR_AUTH:
		case MOSQ_ERR_ACL_DENIED:
		case MOSQ_ERR_UNKNOWN:
		case MOSQ_ERR_ERRNO:
		case MOSQ_ERR_EAI:
		case MOSQ_ERR_PROXY:
		case MOSQ_ERR_PLUGIN_DEFER:
		case MOSQ_ERR_KEEPALIVE:
		case MOSQ_ERR_LOOKUP:
			MQTT_LOG_ERROR("other MOSQ_ERR: %d", ret);
			break;
	}

}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	int rc = 0;

	MQTT_LOG_INFO("mosq point -> %d", (unsigned int)mosq);
	MQTT_LOG_INFO("got message '%.*s' from topic '%s'", message->payloadlen, (char*) message->payload, message->topic);
	
	gjsonMessageCallback(message->payload,message->payloadlen, MESSAGE_MQTT);
	
	/*
	char json[] = "{ \"ar\": [\"1e23\", \"45s6\", \"78v9\"] }";

	json_object *objj;

	if(gjson_string_to_json(json, strlen(json), &objj)==0)
	{
		array_list *arr = gjson_get_array(objj, "ar");
		
		if(arr) {
			MQTT_LOG_INFO("length %d size %d", arr->length, arr->size);
			
			
			MQTT_LOG_INFO("%s", json_object_to_json_string(array_list_get_idx(arr, 0)));
			MQTT_LOG_INFO("%s", json_object_to_json_string(array_list_get_idx(arr, 1)));
			MQTT_LOG_INFO("%s", json_object_to_json_string(array_list_get_idx(arr, 2)));

		}
				
	}*/
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	MQTT_LOG_INFO("Connection with broker is established");
	MQTT_LOG_INFO("mosq point -> %d", (unsigned int)mosq);

	if(!result){

		int mid;
		int ret = 0;

		connect_flag = 1;
		
		
		char *topic=viewSubscribeTopic(true);
		while(topic)
		{
			MQTT_LOG_INFO("Subscribing topic: %s", topic);
			
			ret = mosquitto_subscribe(mosq, &mid, topic, 0);
			mqttReturnCheck(ret, topic);
			
			topic = viewSubscribeTopic(false);
		}

		uint8_t buf_router_info[512];
		memset(buf_router_info, 0, sizeof(buf_router_info));
		transRouterInfo(buf_router_info);

		MQTT_LOG_INFO("Router info: %s", buf_router_info);
		mqttMessagePublish(buf_router_info);
	}
}



void disconnect_callback(struct mosquitto *mosq, void *obj, int result)
{
	MQTT_LOG_ERROR("Connection with broker is unestablished");

	if(!result){

		connect_flag = 0;
		
		MQTT_LOG_ERROR("disconnect_callback");

	}
}


void publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	MQTT_LOG_INFO("publish callback!");
}

void subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;

	MQTT_LOG_INFO("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		MQTT_LOG_INFO(", %d", granted_qos[i]);
	}
	MQTT_LOG_INFO("\n");
}


void mqttMessagePublish(char *message)
{
	int mid,ret;
	
	char *topic=viewPublishTopic(true);
	while(topic)
	{
		ret = mosquitto_publish(mosq_config, &mid, topic, strlen(message), message, 0, false);
		mqttReturnCheck(ret, message);
		
		topic = viewPublishTopic(false);
	}
		
}


