#include "com.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned int g_system_cpu_arch = ARCH_MIPS;   //获取系统架构,在windows虚拟机运行时识别使用

void getCpuArchInit(void)		//获取系统架构
{
	FILE *std;
	float dy;
	char buf[1024];
	std = popen( "uname  -a", "r" );
	fread(buf, sizeof(char), sizeof(buf), std);
	if(strstr(buf,"i686") || strstr(buf,"i386") || strstr(buf,"x86-64"))
	{
		g_system_cpu_arch = ARCH_X86;
	}
	else if(strstr(buf,"ARM") || strstr(buf,"arm"))
	{
		g_system_cpu_arch = ARCH_ARM;
	}
	else if(strstr(buf,"mips"))
	{
		g_system_cpu_arch = ARCH_MIPS;
	}
	pclose(std);
}


char *get_mac(bool CapsLK)
{
	static char mac[64] = {0};

	unsigned int rvalue = 0;

	if(strlen(mac)!=0) {
		return mac;
	}
	getCpuArchInit();

	if(g_system_cpu_arch == ARCH_MIPS) {
		char mac_cmd[2][128] = {"hexdump -v -n 6 -s 4 -e '6/1 \"%02x\"' /dev/mtd2 2>/dev/null", 
								"hexdump -v -n 6 -s 4 -e '6/1 \"%02X\"' /dev/mtd2 2>/dev/null"};

		memset(mac, 0, sizeof(mac));
		// char mac_cmd[] = "hexdump -v -n 6 -s 4 -e '5/1 \"%02X:\" 1/1 \"%02x\"' /dev/mtd2 2>/dev/null";

		command_output(mac_cmd[CapsLK], mac);
	} else {
		
		srand((unsigned)time(NULL));

       	rvalue = rand();

		sprintf(mac, "%d", rvalue);

		//LOGE("not get mac, rand generate");
	}

	return mac;
}

void random_generator(char *buf, int len)
{
	int i = 0;
	char random_pool[]=
	{
		'0','1','2','3','4','5','6','7','8','9',
		'a','b','c','d','e','f','g','h','i','j',
		'k','l','m','n','o','p','q','r','s','t',
		'u','v','w','x','y','z'
	};
	char *p = buf;

	if (len <= 0) {
		return;
	}

	srand((unsigned)time(NULL));

	for (i = 0; i < len; i++) {
		*p++ = random_pool[rand()%sizeof(random_pool)];
	}

	*p = '\0';
}

void file_md5(const char *file, char *md5)
{
	int i = 0;
	uint8_t buf[16] = {0};

	md5sum(file, buf);

	for (i = 0; i < 16; i++) {
		sprintf(md5 + (i * 2), "%02x", (uint8_t) buf[i]);
	}
}


int do_reboot(void)
{
	return exec_command("reboot");
}


int msgid_generator(void)
{
	srand((unsigned)time(NULL));

	return rand()%65536;
}
	
	
#define COMPUTE_BUILD_YEAR \
    ( \
        (__DATE__[ 7] - '0') * 1000 + \
        (__DATE__[ 8] - '0') *  100 + \
        (__DATE__[ 9] - '0') *   10 + \
        (__DATE__[10] - '0') \
    )


#define COMPUTE_BUILD_DAY \
    ( \
        ((__DATE__[4] >= '0') ? (__DATE__[4] - '0') * 10 : 0) + \
        (__DATE__[5] - '0') \
    )


#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define COMPUTE_BUILD_MONTH \
    ( \
        (BUILD_MONTH_IS_JAN) ?  1 : \
        (BUILD_MONTH_IS_FEB) ?  2 : \
        (BUILD_MONTH_IS_MAR) ?  3 : \
        (BUILD_MONTH_IS_APR) ?  4 : \
        (BUILD_MONTH_IS_MAY) ?  5 : \
        (BUILD_MONTH_IS_JUN) ?  6 : \
        (BUILD_MONTH_IS_JUL) ?  7 : \
        (BUILD_MONTH_IS_AUG) ?  8 : \
        (BUILD_MONTH_IS_SEP) ?  9 : \
        (BUILD_MONTH_IS_OCT) ? 10 : \
        (BUILD_MONTH_IS_NOV) ? 11 : \
        (BUILD_MONTH_IS_DEC) ? 12 : \
        /* error default */  99 \
    )

#define COMPUTE_BUILD_HOUR ((__TIME__[0] - '0') * 10 + __TIME__[1] - '0')
#define COMPUTE_BUILD_MIN  ((__TIME__[3] - '0') * 10 + __TIME__[4] - '0')
#define COMPUTE_BUILD_SEC  ((__TIME__[6] - '0') * 10 + __TIME__[7] - '0')

#define BUILD_DATE_IS_BAD (__DATE__[0] == '?')

#define PRE_BUILD_YEAR  ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_YEAR)
#define PRE_BUILD_MONTH ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_MONTH)
#define PRE_BUILD_DAY   ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_DAY)

#define BUILD_TIME_IS_BAD (__TIME__[0] == '?')

#define PRE_BUILD_HOUR  ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_HOUR)
#define PRE_BUILD_MIN   ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_MIN)
#define PRE_BUILD_SEC   ((BUILD_TIME_IS_BAD) ? 99 :  COMPUTE_BUILD_SEC)
	
#define BUILD_VERSION(BUFF) \
		{						\
				sprintf((BUFF), "%d%02d%02d%02d%02d",PRE_BUILD_YEAR,PRE_BUILD_MONTH,PRE_BUILD_DAY,PRE_BUILD_HOUR,PRE_BUILD_MIN);	\
		}


void build_time_get(char *buf)
{
	if(buf!=NULL)
	{
		BUILD_VERSION(buf);
	}
}
