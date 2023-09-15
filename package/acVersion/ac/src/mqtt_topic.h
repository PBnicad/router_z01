#ifndef MQTT_TOPIC_H__
#define MQTT_TOPIC_H__

#include <stdbool.h>

void subscribeTopicAppend(char *topic);
void subscribeTopicDelete(char *topic);
void publishTopicAppend(char *topic);
void publishTopicDelete(char *topic);

/* 遍历主题使用, 类似strtok的用法, 第一次调用first传true, 
    后续一直调用实现遍历直至返回null 
	参考:
	char *topic=viewSubscribeTopic(true);
	while(topic) {
		printf("view topic %s\n", topic);
		topic = viewSubscribeTopic(false);
	}
*/
char *viewSubscribeTopic(bool first);
char *viewPublishTopic(bool first);



#endif

