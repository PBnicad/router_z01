#include "rdss_uart.h"
#include "serial.h"

#include <libubox/md5.h>
#include <libubox/list.h>
#include <libubox/utils.h>
#include <libubox/uloop.h>
#include <json-c/json.h>

#include <syslog.h>
#include <stdint.h>
#include "guart.h"
#include "com.h"
#include "gjson.h"
#include "packet.h"


#define    RDSS_RN_OPEN     "$CCRMO,,4,1*79\r\n"     //打开RN语句
#define    RDSS_RN_CLOSE    "$CCRMO,,3,*4F\r\n"      //关闭RN语句
#define    RDSS_RMC_OPEN    "$CCRMO,RMC,2,1*23\r\n"  //打开RMC语句，频度为1秒
#define    RDSS_RMC_CLOSE   "$CCRMO,RMC,1,1*20\r\n"  //关闭RMC语句
#define    RDSS_GGA_OPEN    "$CCRMO,GGA,2,1*3E\r\n"  //打开GGA语句，频度为1秒
#define    RDSS_GGA_CLOSE   "$CCRMO,GGA,1,1*3D\r\n"  //关闭GGA语句
#define    RDSS_GLL_OPEN    "$CCRMO,GLL,2,1*38\r\n"  //打开GLL语句，频度为1秒
#define    RDSS_GLL_CLOSE   "$CCRMO,GLL,1,1*3B\r\n"  //关闭GLL语句
#define    RDSS_GSA_OPEN    "$CCRMO,GSA,2,1*2A\r\n"  //打开GSA语句，频度为1秒
#define    RDSS_GSA_CLOSE   "$CCRMO,GSA,1,1*29\r\n"  //关闭GSA语句
#define    RDSS_GSV_OPEN    "$CCRMO,GSV,2,1*3D\r\n"  //打开GSV语句，频度为1秒
#define    RDSS_GSV_CLOSE   "$CCRMO,GSV,1,1*3E\r\n"  //关闭GSV语句

#define    RDSS_DEV_INFO    "$CCICA,0,00*7B\r\n"     //设备信息检测

#define    RDSS_BAUD_SET(BAUD)  "$CCCOM,"##BAUD##",8,1,0*7F\r\n"
#define    RDSS_TXA_SEND(STR)    "$CCTXA,0166166,1,1,333033313332333333343335333633373338333933303331333233333334333533363337333833393330333133323333333433353336333733383339333033313332333333343335333633373338*77\r\n"

struct uloop_fd rdss_ul_fd;
struct uloop_timeout rdss_ul_timer;
struct uloop_timeout rdss_resend_timer;
char rdss_info_ica[128] = {0};
char rdss_send_ic[128] = "0934845";

static void rdss_tty_recv_cb(struct uloop_fd *fd, unsigned int events)
{
	uint8_t buf[2048] = {0};

	ssize_t r = read(fd->fd, buf, sizeof(buf));
	if (r < 0) {
		LOGI("Fail to read message from serial COM\n");
		return;
	}
//	LOGE("%s", buf);
	void module_write_byte(char byte);
	for (int i = 0; i < r; i++)
	{
		module_write_byte(buf[i]);
	}
}

/*
void rdss_generate_send(unsigned char *buf, unsigned char *send_str)
{
	unsigned int len = 0;
	unsigned int temp = 0;
	len = sprintf(buf, "$CCTXA,0166166,1,1,%s",send_str);
	for(int i=1; i<strlen(buf);i++)
	{
		temp = temp ^ (int)buf[i];
	}
	sprintf(&buf[strlen(buf)],"*%02X",(int)temp);//
}
*/

static void transHexToAsiic(unsigned char *asiic, unsigned char *hex, int len)
{
	if(hex==NULL || asiic==NULL){
			return ;
	}
	for(int i=0;i<len;i++) {
		sprintf(&asiic[2*i], "%02X", hex[i]);
	}
	printf("%s\n",asiic);
}

static void transAsiicToHex(unsigned char *asiic, unsigned char *hex, int len)
{
	if(hex==NULL || asiic==NULL){
		return ;
	}
	for(int i=0;i<len;i++) {
		sscanf(&asiic[i*2], "%02hhX", &hex[i]);
	}
}

void rdss_generate_send(unsigned char *buf, unsigned char *send_str)
{
	unsigned int len = 0;
	unsigned int temp = 0;
	len = sprintf(buf, "$CCTXA,%s,1,2,A4%s", rdss_send_ic, send_str);
	for(int i=1; i<strlen(buf);i++)
	{
		temp = temp ^ (int)buf[i];
	}
	sprintf(&buf[strlen(buf)],"*%02X\r\n",(int)temp);//
}

void rdss_generate_time_send(void)
{
	unsigned char buf[1024] = {0};
	unsigned char send_text[128] = {0};
	
	time_t times = 0;
	time(&times);
	char *t = ctime(&time);
	transHexToAsiic(send_text, t, strlen(t));
	rdss_generate_send(buf, send_text);
	ssize_t s = writen(rdss_ul_fd.fd, buf, strlen(buf));
	if (s < 0) {
		LOGE("Fail to send data to ttyS2");
	}
	LOGE("send local time:%s", buf);
}

void rdss_timer_cb(struct uloop_timeout *t)
{
/*	ssize_t s = writen(rdss_ul_fd.fd, RDSS_DEV_INFO, strlen(RDSS_DEV_INFO));
	if (s < 0) {
		LOGE("Fail to send data to ttyS2, packet will be drop\n");
	}
	//
	char buf[1024] = {0};
	rdss_generate_send(buf, "313233343536373839")
	
	LOGE("w:%s", buf);
	s = writen(rdss_ul_fd.fd, buf, strlen(buf));
	if (s < 0) {
		LOGE("Fail to send data to ttyS2, packet will be drop\n");
	}*/
	rdss_generate_time_send();
	//uloop_timeout_cancel(&rdss_resend_timer);
	uloop_timeout_set(&rdss_ul_timer, 40005);
}

void rdss_resend_cb(struct uloop_timeout *t)
{
	rdss_generate_time_send();
}


void rdss_uart_init(void)
{
	char *device      = "/dev/ttyS2";
	int baudrate      = 115200;
	int rdss_tty_fd= 0;

	rdss_tty_fd = open_tty(device, baudrate, false);

	if (rdss_tty_fd < 0) {
		LOGE("open tty fail");
		return ;
		//exit(EXIT_FAILURE);
	}

	rdss_ul_fd.fd = rdss_tty_fd;
	rdss_ul_fd.cb = rdss_tty_recv_cb;

	rdss_ul_timer.cb = rdss_timer_cb;
	rdss_resend_timer.cb = rdss_resend_cb;
	
	void rdss_module_init(void);
	rdss_module_init();
	
	uloop_fd_add(&rdss_ul_fd, ULOOP_READ | ULOOP_EDGE_TRIGGER);
	uloop_timeout_set(&rdss_ul_timer, 10000);
}



#define MODOLE_DATA_BUFFER_SIZE  1024
#define MODOLE_CMD_BUFFER_SIZE   128
#define MODOLE_WRITE_BUFFER_SIZE 128

typedef enum{
	DATA_OVERFLOW,
	DATA_DONE,
}rec_status;
void module_write_byte(char byte);

typedef struct {
	char *data_buffer;            /*数据缓存*/
	char *cmd_buffer;             /*命令缓存*/
	char *w_buffer;               /*写入缓存*/
	void(*write)(char byte);      /*写数据*/
	void(*data_handle)(char *data, unsigned int length, rec_status status);  /*数据接收回调*/
	void(*cmd_handle)(char *data, unsigned int length, rec_status status);   /*命令接收回调*/
	unsigned short w_length;
	unsigned short cmd_max_len;
	unsigned short data_max_len;
	unsigned short w_max_len;
	unsigned char rec_dir;
	const unsigned char *data_head;
}module_info;

module_info my_module = {
	.data_buffer = NULL,
	.cmd_buffer = NULL,
	.write = module_write_byte,
	.w_length = 0,
	.data_head = NULL,
};

void module_write_byte(char byte)
{
	if (my_module.data_buffer==NULL || \
		my_module.cmd_buffer==NULL || \
		my_module.w_buffer == NULL || \
		(my_module.w_length <= 2 && (byte == '\r' || byte == '\n')))
	{
		return;
	}
	if (my_module.rec_dir)  /*数据帧*/
	{
		my_module.data_buffer[my_module.w_length] = byte;
		my_module.w_length++;
		if (byte == '\n')
		{
			my_module.data_buffer[my_module.w_length] = '\0';
			my_module.w_length++;
			if (my_module.data_handle)
			{
				my_module.data_handle(my_module.data_buffer, my_module.w_length, DATA_DONE);
			}
			my_module.w_length = 0;
			my_module.rec_dir = 0;
		}
		else if (my_module.data_max_len <= my_module.w_length+1)/*检验是否溢出*/
		{
			my_module.data_buffer[my_module.w_length] = '\0';
			if (my_module.data_handle)
			{
				my_module.data_handle(my_module.data_buffer, my_module.data_max_len, DATA_OVERFLOW);
			}
			my_module.w_length = 0;
			my_module.rec_dir = 0;
		}
	}
	else      /*命令帧*/
	{
		my_module.w_buffer[my_module.w_length] = byte;
		my_module.w_length++;

		/*检验是否是数据帧*/
		if (my_module.data_head != NULL)
		{
			unsigned char len = 0;
			unsigned char ret = 0;
			len = strlen(my_module.data_head);
			if (my_module.w_length == len)
			{
				ret = strncmp(my_module.data_head, my_module.w_buffer, len);
				if (ret == 0)
				{
					my_module.rec_dir = 1;
					memcpy(my_module.data_buffer, my_module.w_buffer, len);
					return;
				}
			}
		}

		/*检验是否接收完成*/
		if (byte == '\n')
		{
			my_module.w_buffer[my_module.w_length] = '\0';
			my_module.w_length++;
			memcpy(my_module.cmd_buffer, my_module.w_buffer, my_module.w_length);
			if (my_module.cmd_handle)
			{
				my_module.cmd_handle(my_module.cmd_buffer, my_module.w_length, DATA_DONE);
			}
			my_module.w_length = 0;
			my_module.rec_dir = 0;
		}
		else if (my_module.w_max_len <= my_module.w_length+1)/*检验是否溢出*/
		{
			my_module.w_buffer[my_module.w_length] = '\0';
			memcpy(my_module.cmd_buffer, my_module.w_buffer, my_module.cmd_max_len);
			if (my_module.cmd_handle)
			{
				my_module.cmd_handle(my_module.cmd_buffer, my_module.cmd_max_len, DATA_OVERFLOW);
			}
			my_module.w_length = 0;
			my_module.rec_dir = 0;
		}
	}
}

/*
注册数据命令头信息，通过该信息，将会把该帧数据缓存到data_buffer并触发数据接收
*/
void module_register_data_head(const char *head)
{
	if (head != NULL)
	{
		my_module.data_head = head;
	}

}

void module_register_data_handle(void(*data_handle)(char *data, unsigned int length, rec_status status))
{
	if (data_handle != NULL)
	{
		my_module.data_handle = data_handle;
	}
}
void module_register_cmd_handle(void(*cmd_handle)(char *data, unsigned int length, rec_status status))
{
	if (cmd_handle != NULL)
	{
		my_module.cmd_handle = cmd_handle;
	}
}


void module_register_data_buffer(char *buffer, unsigned short length)
{
	if (buffer != NULL)
	{
		my_module.data_max_len = length;
		my_module.data_buffer = buffer;
	}
}

void module_register_cmd_buffer(char *buffer, unsigned short length)
{
	if (buffer != NULL)
	{
		my_module.cmd_max_len = length;
		my_module.cmd_buffer = buffer;
	}
}

void module_register_write_buffer(char *buffer, unsigned short length)
{
	if (buffer != NULL)
	{
		my_module.w_max_len = length;
		my_module.w_buffer = buffer;
	}
}

#define MODOLE_DATA_BUFFER_SIZE  1024
#define MODOLE_CMD_BUFFER_SIZE   128
#define MODOLE_WRITE_BUFFER_SIZE 128
char data_buf[MODOLE_DATA_BUFFER_SIZE] = { 0 };
char cmd_buf[MODOLE_CMD_BUFFER_SIZE] = { 0 };
char w_buf[MODOLE_WRITE_BUFFER_SIZE] = { 0 };

/**
$CCTXA,0166166,1,1,3132333433333335*7F  发起通讯
$BDFKI,TXA,N,Y,0,0000*04                申请失败
$BDFKI,TXA,Y,Y,0,0040*17                申请成功
$BDTXR,1,0166166,1,,3132333433333335*46 收到数据
$CCTXA,0166166,1,1,3132333433333335*7F  发起通讯
$BDFKI,TXA,Y,Y,0,0040*17                申请成功
$BDICI,0166166,02097151,0000011,6,40,3,N,000*33   读取到卡号
$CCRMO,BSI,2,10*17
$CCBSS,05,03*44
$CCBSS,,*42
*/

#define CSTR(STR) (STR[0]+STR[1]+STR[2]+STR[3]+STR[4])

void data_handle(char *data, unsigned int len, rec_status status)
{
	if (status == DATA_DONE)
	{
		LOGE("data:%s", data);
		unsigned int sw = 0;
		unsigned int ret = 0;
		char *key = NULL;
		char *key_exe = NULL;
		char *key_f = NULL;
		char *key_wait = NULL;
		sw = data[1]+data[2]+data[3]+data[4]+data[5];
		switch (sw)
		{
			case 'B'+'D'+'I'+'C'+'I':
				ret = sscanf(data, "$BDICI,%[^,]s,%*[^\n]s", rdss_info_ica);
				if(ret == 1)
					LOGE("IC:%s", rdss_info_ica);
			break;
			case 'B'+'D'+'T'+'X'+'R':
				LOGE("rec message");
			break;
			case 'B'+'D'+'F'+'K'+'I':
				key = strsep(&data, ",");
				key = strsep(&data, ",");
				if(strcmp(key, "TXA")==0) {
					key_exe = strsep(&data, ",");  /*执行情况 Y/N*/
					key_f = strsep(&data, ",");    /*频度情况 Y/N*/
					key = strsep(&data, ",");      /*发射抑制情况,正常为"0"*/
					key_wait = strsep(&data, ","); /*等待时间*/
					LOGE("%s|%s|%s|%d", key_exe, key_f, key, atoi(key_wait));
					if(key_exe[0]=='N' || key_f[0]=='N') {
						//发送失败
						unsigned int wt = atoi(key_wait);
						if(wt) {
							uloop_timeout_set(&rdss_resend_timer, wt*1000-950);
							LOGE("TXA Y/N fail, resend delay %dms", wt*1000-950);
						}
						return;
					}
					if(key[0]!='0') {
						//发送失败
						LOGE("TXA send fail");
						return;
					}
				}
			break;
		}
	}
	else
	{
		LOGE("data flow:%s", data);
	}
}

void cmd_handle(char *data, unsigned int len, rec_status status)
{
	if (status == DATA_DONE)
	{
		LOGE("cmd:%s", data);
	}
	else
	{
		LOGE("cmd flow:%s", data);
	}
}


void rdss_module_init(void)
{
	static const char head[] = "$";
	module_register_data_buffer(data_buf, sizeof(data_buf));
	module_register_cmd_buffer(cmd_buf, sizeof(cmd_buf));
	module_register_write_buffer(w_buf, sizeof(w_buf));
	module_register_data_head(head);
	module_register_data_handle(data_handle);
	module_register_cmd_handle(cmd_handle);
}


void rdssJsonDeal(unsigned char *json_data, unsigned int json_data_len, unsigned char *toBuff, struct mosquitto *mosq)
{
	
	json_object *jobj;
	ssize_t s;
	char buf[1024] = {0};
	
	extern int string_to_json(char *string, unsigned long len, json_object **json);
	if(string_to_json((char *)json_data, json_data_len, &jobj)==0) {
		LOGE("rdss string to json success\n");
		const char *data = gjson_get_string(jobj, "rdss");
		if(data!=NULL) {
			strcpy(buf, data);
			strcat(buf, "\r\n");
			LOGE("Write:%s", buf);
			s = writen(rdss_ul_fd.fd, buf, strlen(buf));
			if (s < 0) {
				LOGE("Fail to send data to ttyS2, packet will be drop\n");
			}

		} else {
			data = gjson_get_string(jobj, "rdssic");
			if(data!=NULL) {
				memset(rdss_send_ic, 0, sizeof(rdss_send_ic));
				strcpy(rdss_send_ic, data);
			}
			data = gjson_get_string(jobj, "rdssp");
			if(data!=NULL) {
				rdss_generate_send(buf, data);
				LOGE("Write:%s", buf);
				s = writen(rdss_ul_fd.fd, buf, strlen(buf));
				if (s < 0) {
					LOGE("Fail to send data to ttyS2, packet will be drop\n");
				}
			}
		}
	} else {
		LOGE("rdss string to json fail\n");
	}
}



