#include "upgrade.h"

extern ENVIRONMENT_STP_t g_Stp_env_t;

uint32_t upgrade_file_size = 0;
uint32_t upgrade_read_size = 0;

#define	FILE_ROW_SIZE	73

UPGRADE_STEP_STATUS_t g_upgrade_status_t = {
	.step = 0,
	.status = UPGRADE_STATUS_NONE,
};

extern ENVIRONMENT_STP_t g_Stp_env_t;


int download_file(const char *url, const char *out_file, int timeout_ms)
{
	int rc = 0;
    FILE *fp = NULL;

    CURL *curl;
    CURLcode res;

	if (!url || !out_file)
		return -1;

	curl = curl_easy_init();
	if (!curl)
		return -1;

	fp = fopen(out_file, "wb");
	if (!fp) {
		rc = -1;
		goto out;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		rc = -1;
	}

out:
	if (curl)
		curl_easy_cleanup(curl);

	if (fp)
		fclose(fp);

	return rc;
}

uint8_t upgradeTransToHex(uint8_t *outbuf, uint16_t bin_len, uint8_t *buf_bin)
{
	uint8_t i = 0;
	while (bin_len<500) {	//1-10 > 500 (unsigned)
		sscanf(outbuf, "%hhx", &buf_bin[i]);
		outbuf += 3;
		bin_len -= 3;
		i++;
	}
	return i;
}

int upgradeFileSize(char* filename)
{    
	FILE *fp=fopen(filename,"r");
	if(!fp) return -1;
	fseek(fp,0L,SEEK_END);
	int size=ftell(fp);
	fclose(fp);
	return size;
}

uint16_t upgradeUwbFirmwareStep(uint8_t *hex_buf)
{
	uint8_t r_buf[128] = {0};
	uint16_t r_size = 0, hex_size;
	FILE *fp;
	fp = fopen(FILE_OUTPUT, "r");
	if(!fp) {
		
		if(debug)
			ULOG_INFO("upgrade file not found, stop upgrade");
		g_Stp_env_t.flag_upgrade_mode = UPGRADE_STOP;
		g_upgrade_status_t.step=0;
		g_upgrade_status_t.status=UPGRADE_STATUS_NONE;
		return 0;
	}
	fseek(fp, g_upgrade_status_t.step*FILE_ROW_SIZE, SEEK_SET);	//偏移位置
	//读取一行数据, 包括回车换行
	r_size = fread(r_buf, 1, FILE_ROW_SIZE, fp);
	upgrade_read_size += r_size;
	hex_size = upgradeTransToHex(r_buf, r_size, hex_buf);
/*	if(g_upgrade_status_t.step<10 || r_size<=FILE_ROW_SIZE) {
		uint8_t buf[512] = {0};
		fprintf(buf, "echo \"size%d:%d %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx \">>/root/9.txt", r_size, hex_size, \
				hex_buf[0], hex_buf[1], hex_buf[2], hex_buf[3], hex_buf[4], hex_buf[5], hex_buf[6], hex_buf[7],\
				 hex_buf[8], hex_buf[9], hex_buf[10], hex_buf[11], hex_buf[12], hex_buf[13], hex_buf[14], hex_buf[15],\
				  hex_buf[16], hex_buf[17], hex_buf[18], hex_buf[19], hex_buf[20], hex_buf[21], hex_buf[22], hex_buf[23]);
		system(buf);
	}*/
	g_upgrade_status_t.step++;
	fclose(fp);
	return hex_size;
}


void *pthreadUpgrade(void *arg)
{

	uint8_t url[128] = {0};
	strcpy(url, arg);
	if(debug)
		ULOG_INFO("download: %s", (char *)arg);
	unlink(FILE_OUTPUT);	//删除文件,防止有损坏文件导致下载失败
	if(download_file(url,FILE_OUTPUT, 5000)<0) {
		g_Stp_env_t.flag_upgrade_mode = UPGRADE_STOP;	//下载失败,
	} else {
		g_Stp_env_t.flag_upgrade_mode = UPGRADE_START;	//下载完成, 标记开始升级
		upgrade_file_size = upgradeFileSize(FILE_OUTPUT);
		upgrade_read_size = 0;
	}
}

bool upgradeCheck(uint8_t *buf, uint16_t len)
{
	uint8_t url[128] = {0};
	pthread_t ntid;
	if(strncmp(buf, "upgrade http", 12)==0) {
		sscanf(buf, "%*s%s", url);
		if(debug)
			ULOG_INFO("down url %s", buf);
		//create thread 
		pthread_create(&ntid, NULL, pthreadUpgrade, url);	//创建线程下载固件
		return true;
	} else {
		return false;
	}
}



