#ifndef MQTT_TOPIC_H__
#define MQTT_TOPIC_H__

#include <stdbool.h>

void subscribeTopicAppend(char *topic);
void subscribeTopicDelete(char *topic);
void publishTopicAppend(char *topic);
void publishTopicDelete(char *topic);

/* ��������ʹ��, ����strtok���÷�, ��һ�ε���first��true, 
    ����һֱ����ʵ�ֱ���ֱ������null 
	�ο�:
	char *topic=viewSubscribeTopic(true);
	while(topic) {
		printf("view topic %s\n", topic);
		topic = viewSubscribeTopic(false);
	}
*/
char *viewSubscribeTopic(bool first);
char *viewPublishTopic(bool first);



#endif

