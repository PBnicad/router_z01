#include "udp_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include <net/if.h>

/*
创建2个socket 一个加入组播网络负责接收数据(使用本地端口6696),一个负责发送数据到组播网络(使用本地端口6697)
为什么要创建2个socket而不使用一个? 使用一个socket会异常
*/


#include "shell.h"
#include "utils.h"
#include "com.h"
#include "gjson.h"
#include "udp_data_center.h"

#define UDP_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define UDP_LOG_ERROR(...) {LOGE(__VA_ARGS__);}

#define SHARE_PORT 6696

#define INTERFACE "eth0.2"

#define GROUP "239.0.0.18"

// top -n1 | fgrep CPU: | fgrep usr | awk '{print $8}'   view cpu idel 

static char local_ip[20] = {0};
static volatile unsigned char hex_ip[4] = {0};
static volatile unsigned char hex_mac[6] = {0};
static char all_ip[100] = {0};

pthread_mutex_t wait_ready_mutex;

const unsigned char *udpGetMac(void)
{
	return (const unsigned char *)hex_mac;
}
const unsigned char *udpGetIp(void)
{
	return (const unsigned char *)hex_ip;
}
const char *updGetAllIpString(void)
{
	//all_ip[0] is ','
	return (const char *)&all_ip[1];
}

void udpConfigIp(unsigned char ip_of)
{
	char mac[20] = {0};
	char *p_mac_str = get_mac(0);
	memcpy(mac, p_mac_str, strlen(p_mac_str));
	sscanf(mac, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx", &hex_mac[0], &hex_mac[1], &hex_mac[2], &hex_mac[3], &hex_mac[4], &hex_mac[5]);
	
	char cmd_out[512] = {0};
	char *inet = NULL;
	char *start_scan = cmd_out;
	unsigned char ip[5] = {0};
	char cmd_exe[512] = {0};
	
	int ip_count = 0;
	
	
	snprintf(cmd_exe, sizeof(cmd_exe), "ip a | grep %s | grep inet", INTERFACE);
	command_output(cmd_exe, cmd_out);
	
	memset(all_ip, 0, sizeof(all_ip));
	while(1)
	{
		inet = strstr(start_scan, "inet");
		if(inet)
		{
			start_scan = inet + 1;
			sscanf(inet, "inet %hhd.%hhd.%hhd.%hhd/%hhd", &ip[0], &ip[1], &ip[2], &ip[3], &ip[4]);
			UDP_LOG_INFO("parse ip %d.%d.%d.%d/%d", ip[0], ip[1], ip[2], ip[3], ip[4]);
			if(ip[0]==169 && ip[1]==254)
			{
				memset(cmd_exe, 0, sizeof(cmd_exe));
				snprintf(cmd_exe, sizeof(cmd_exe), "ip a delete %d.%d.%d.%d/%d dev %s", ip[0], ip[1], ip[2], ip[3], ip[4], INTERFACE);
				exec_command(cmd_exe);
				UDP_LOG_INFO("%s", cmd_exe);
			}
			else
			{
				//ip_count++;
				memset(local_ip, 0, sizeof(local_ip));
				snprintf(local_ip, sizeof(local_ip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
				
			}
			
			snprintf(&all_ip[strlen(all_ip)], sizeof(all_ip)-strlen(all_ip), ",%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
		}
		else
		{
			break;
		}
	}
	if(ip_count==0)
	{
		memset(cmd_exe, 0, sizeof(cmd_exe));
		snprintf(cmd_exe, sizeof(cmd_exe), "ip a add 169.254.%d.%d/16 dev %s", hex_mac[4], hex_mac[5]+ip_of, INTERFACE);
		UDP_LOG_INFO("%s", cmd_exe);
		exec_command(cmd_exe);
		memset(local_ip, 0, sizeof(local_ip));
		sprintf(local_ip, "169.254.%d.%d", hex_mac[4], hex_mac[5]+ip_of);
		//hex(local_ip, 20);
	}
	UDP_LOG_INFO("%s ip %s", INTERFACE, local_ip);
	//169.254.x.x
	sscanf(local_ip, "%hhd.%hhd.%hhd.%hhd", &hex_ip[0], &hex_ip[1], &hex_ip[2], &hex_ip[3]);
	hex(local_ip, 20);
	UDP_LOG_INFO("hex ip %d.%d.%d.%d", hex_ip[0], hex_ip[1], hex_ip[2], hex_ip[3]);
	UDP_LOG_INFO("hex mac %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", hex_mac[0], hex_mac[1], hex_mac[2], hex_mac[3],hex_mac[4], hex_mac[5]);
}

void hex(unsigned char *data, unsigned int size)
{
	char buf[1024] = {0};
	
	for(int i=0; i<size; i++)
	{
		sprintf(&buf[strlen(buf)], "%02hhx ", data[i]);
	}
	sprintf(&buf[strlen(buf)], "\n");
	UDP_LOG_INFO("%s", buf);
}

void *udpPthreadGroupRecvHandler(void *p_context)
{
	pthread_mutex_lock(&wait_ready_mutex);
	
	static volatile int udp_socket_fd;

    struct sockaddr_in localaddr;
    struct sockaddr_in cli_addr;    //獲取客戶端地址信息
	
    int ret;
    int len;
    char buf[BUFSIZ];

    struct ip_mreqn group;          
    struct in_addr addr;                                        /* 组播结构体 */

    udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket_fd<0)
    {
        UDP_LOG_INFO("socket create failure\n");
		pthread_exit((void*)0);
    }

    bzero(&localaddr, sizeof(localaddr));                                   /* 初始化 */
    localaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0" , &localaddr.sin_addr.s_addr);
    localaddr.sin_port = htons(SHARE_PORT);

    ret = bind(udp_socket_fd, (struct sockaddr *)&localaddr, sizeof(localaddr));
    if(ret < 0)
    {
        UDP_LOG_INFO("socket bind failure\n");
		pthread_exit((void*)0);
    }

    inet_pton(AF_INET, GROUP, &group.imr_multiaddr.s_addr);                    /* 设置组地址 */
    inet_pton(AF_INET, "0.0.0.0", &group.imr_address);                         /* 使用本地任意IP添加到组播组 */
    group.imr_ifindex = if_nametoindex(INTERFACE);                             /* 通过网卡名-->编号 ip ad */
    
    setsockopt(udp_socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));/* 设置client 加入多播组 */

	socklen_t socklen = sizeof(cli_addr);
	
	pthread_mutex_unlock(&wait_ready_mutex);
	
	UDP_LOG_INFO("socket start recvfrom message, max buff size %d\n", BUFSIZ);
	
    while (1) {
        //UDP_LOG_INFO("[%d]Waiting for data from sender\n", udp_socket_fd);
		memset(buf, 0, sizeof(buf));
        len = recvfrom(udp_socket_fd, buf, sizeof(buf), 0, (struct sockaddr*)&cli_addr, &socklen);
		if(len > 0) {
			addr.s_addr = cli_addr.sin_addr.s_addr;
			//hex(buf, len);
			//UDP_LOG_INFO("[%s]Receive message from %s %d: %s\n", get_mac(0), inet_ntoa(addr), cli_addr.sin_port, buf);
			udpMessageHandler(buf, len);
		} else {
			//网络异常时重连,未实现
			UDP_LOG_INFO("[%s]Receive error, need reconnect.", get_mac(0));
			close(udp_socket_fd);
			break;
		}
    }
    close(udp_socket_fd);

    pthread_exit((void*)0);
}

static volatile int udp_socket_write_fd;


void udpSocketSendMessage(unsigned char *data, unsigned int size)
{
    struct sockaddr_in clientaddr;
	
    bzero(&clientaddr, sizeof(clientaddr));                 /* 构造 client 地址 IP+端口 */
    clientaddr.sin_family = AF_INET;
    inet_pton(AF_INET, GROUP, &clientaddr.sin_addr.s_addr); /* IPv4  239.0.0.2+9000 */
    clientaddr.sin_port = htons(SHARE_PORT);
	
	if(sendto(udp_socket_write_fd, data, size, 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0)
	{
		UDP_LOG_INFO("[%s:%d]udp send design message fail %d %s", local_ip, udp_socket_write_fd, errno, strerror(errno));
		/*reconnect*/
		udpConfigIp(0);
	}
	else
	{
		UDP_LOG_INFO("[%d]send design pack size %d", udp_socket_write_fd, size);
	}
}

void updRefreshAllIp(void)
{
	char cmd_out[256] = {0};
	char *inet = NULL;
	char *start_scan = cmd_out;
	unsigned char ip[5] = {0};
	char cmd_exe[64] = {0};
	
	snprintf(cmd_exe, sizeof(cmd_exe), "ip a | grep %s | grep inet", INTERFACE);
	
	command_output(cmd_exe, cmd_out);
	memset(all_ip, 0, sizeof(all_ip));
	start_scan = cmd_out;
	while(1)
	{
		inet = strstr(start_scan, "inet");
		if(inet)
		{
			start_scan = inet + 1;
			sscanf(inet, "inet %hhd.%hhd.%hhd.%hhd/%hhd", &ip[0], &ip[1], &ip[2], &ip[3], &ip[4]);
			//UDP_LOG_INFO("parse ip %d.%d.%d.%d/%d", ip[0], ip[1], ip[2], ip[3], ip[4]);
			
			snprintf(&all_ip[strlen(all_ip)], sizeof(all_ip)-strlen(all_ip), ",%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
		}
		else
		{
			break;
		}
	}
}

void *udpPthreadSendHandler(void *p_context)
{
	sleep(1);
	pthread_mutex_lock(&wait_ready_mutex);
	pthread_mutex_unlock(&wait_ready_mutex);
	pthread_mutex_destroy(&wait_ready_mutex);
	
    struct sockaddr_in localaddr;
	int ticks = 0, ret;

    udp_socket_write_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket_write_fd<0)
    {
        UDP_LOG_INFO("socket create failure\n");
		pthread_exit((void*)0);
    }

    bzero(&localaddr, sizeof(localaddr));                                   /* 初始化 */
    localaddr.sin_family = AF_INET;
    inet_pton(AF_INET, local_ip, &localaddr.sin_addr.s_addr);
    localaddr.sin_port = htons(SHARE_PORT+1);

    ret = bind(udp_socket_write_fd, (struct sockaddr *)&localaddr, sizeof(localaddr));
    if(ret < 0)
    {
        UDP_LOG_INFO("socket bind failure\n");
		pthread_exit((void*)0);
    }
	
	
	
	static uint16_t router_report_time  = 3600/30;
	
    while (1) {
		if(router_report_time++>=3600/30) {
			router_report_time = 0;
			
			uint8_t buf_router_info[512];
			memset(buf_router_info, 0, sizeof(buf_router_info));
			transRouterInfo(buf_router_info);

			UDP_LOG_INFO("udp report Router info: %s", buf_router_info);
			udpSocketSendMessage(buf_router_info, strlen(buf_router_info));
		}
		updRefreshAllIp();
		udpMessageHeartbeat();
		sleep(30);
    }

    close(udp_socket_write_fd);
	pthread_exit((void*)0);
}

void udpPthreadCreate(void)
{
	pthread_t p_id;
	
	udpConfigIp(0);
	
	if(pthread_mutex_init(&wait_ready_mutex, NULL) < 0)
		UDP_LOG_INFO("udp wait_ready_mutex init success.");
	
	int err = pthread_create(&p_id, NULL, udpPthreadGroupRecvHandler, NULL);
	if(err==0) {
		UDP_LOG_INFO("udp pthread create success.");
	} else {
		UDP_LOG_INFO("udp pthread create failed.");
	}
	err = pthread_create(&p_id, NULL, udpPthreadSendHandler, NULL);
	if(err==0) {
		UDP_LOG_INFO("udp pthread create success.");
	} else {
		UDP_LOG_INFO("udp pthread create failed.");
	}
}

