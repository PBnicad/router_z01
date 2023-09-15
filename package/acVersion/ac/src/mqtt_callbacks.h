#ifndef MQTT_CALLBACKS_H__
#define MQTT_CALLBACKS_H__

#include <mosquitto.h>

void connect_callback(struct mosquitto *mosq, void *obj, int result);
void disconnect_callback(struct mosquitto *mosq, void *obj, int result);
void publish_callback(struct mosquitto *mosq, void *obj, int mid);
void subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);

void mqttMessagePublish(char *message);

#endif //MQTT_CALLBACKS_H__
