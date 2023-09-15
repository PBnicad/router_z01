#include "mqtt_topic.h"

#include <string.h>
#include "utils.h"

typedef struct {
	char *topic;
	void *next;
}TOPIC_NODE_t;

static TOPIC_NODE_t *g_subscribe_list = NULL;
static TOPIC_NODE_t *g_publish_list = NULL;

void topicAppend(TOPIC_NODE_t **head, char *topic)
{
	TOPIC_NODE_t *node = (TOPIC_NODE_t *)ss_malloc(sizeof(TOPIC_NODE_t));
	char *node_topic = (char *)ss_malloc(strlen(topic)+1);
	
	memset(node, 0, sizeof(TOPIC_NODE_t));
	memset(node_topic, 0, sizeof(strlen(topic)+1));

	strcpy(node_topic, topic);
	
	node->topic = node_topic;
	node->next = NULL;
	
	TOPIC_NODE_t *poll = (*head);
	if(poll == NULL) {
		*head = node;
	} else {
		while(poll->next)
		{
			poll = poll->next;
		}
		poll->next = node;
	}
}

void topicDelete(TOPIC_NODE_t **head, char *topic)
{
	TOPIC_NODE_t *poll = *head;

	if(strcmp(poll->topic, topic)==0) {
		*head = poll->next;
		ss_free(poll->topic);
		ss_free(poll);
		return;
	}

	while(poll->next) {
		TOPIC_NODE_t *node = poll->next;

		if(strcmp(node->topic, topic)==0) {
			poll->next = node->next;
			ss_free(node->topic);
			ss_free(node);
			break;
		}
		poll = poll->next;
	}
	
	
}


static TOPIC_NODE_t *p_view_sub_list = NULL;
static TOPIC_NODE_t *p_view_pub_list = NULL;

char *viewSubTopic(TOPIC_NODE_t *head)
{
	if(head) {
		p_view_sub_list = head;
	}
	
	if(p_view_sub_list) {
		char *topic = p_view_sub_list->topic;
		
		p_view_sub_list = p_view_sub_list->next;

		return topic;
	}
	return NULL;
}

char *viewPubTopic(TOPIC_NODE_t *head)
{
	if(head) {
		p_view_pub_list = head;
	}
	
	if(p_view_pub_list) {
		char *topic = p_view_pub_list->topic;
		
		p_view_pub_list = p_view_pub_list->next;

		return topic;
	}
	return NULL;
}

char *viewSubscribeTopic(bool first)
{
	if(first) {
		return viewSubTopic(g_subscribe_list);
	} else {
		return viewSubTopic(NULL);
	}
}

char *viewPublishTopic(bool first)
{
	if(first) {
		return viewPubTopic(g_publish_list);
	} else {
		return viewPubTopic(NULL);
	}
}

void subscribeTopicAppend(char *topic)
{
	topicAppend(&g_subscribe_list, topic);
}

void subscribeTopicDelete(char *topic)
{
	topicDelete(&g_subscribe_list, topic);
}

void publishTopicAppend(char *topic)
{
	topicAppend(&g_publish_list, topic);
}

void publishTopicDelete(char *topic)
{
	topicDelete(&g_publish_list, topic);
}