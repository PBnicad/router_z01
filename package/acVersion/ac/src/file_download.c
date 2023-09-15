#include "file_download.h"
#include "shell.h"
#include "utils.h"
#include "com.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#define FILE_LOG_INFO(...) {if(access_contrl_config.debug) LOGI(__VA_ARGS__);}
#define FILE_LOG_ERROR(...) {LOGE(__VA_ARGS__);}



typedef struct{
	char url[300];
	char path[100];
	MESSAGE_TYPE type;
}FILE_DOWNLOAD_t;

void (*downloadFileCallbacks)(uint32_t id, int status, char *path, MESSAGE_TYPE type);


void fileDownloadCallbackSet(void (*callback)(uint32_t id, int status, char *path, MESSAGE_TYPE type))
{
	downloadFileCallbacks = callback;
}


void *pthreadDownload(void *arg)
{
	int ret = 0;
	FILE_DOWNLOAD_t *p_evt = (FILE_DOWNLOAD_t *)arg;
	
	FILE_DOWNLOAD_t download = {0};
	memcpy(&download, p_evt, sizeof(FILE_DOWNLOAD_t));
	ss_free(p_evt);
	FILE_LOG_INFO("url: %s, path: %s.", download.url, download.path);
	
	unlink(download.path);
	if(download_file((const char *)download.url, (const char *)download.path, 60000) < 0)
	{
		FILE_LOG_ERROR("Fail to call download_file().");
		if(downloadFileCallbacks)
		{
			downloadFileCallbacks(0, -1, download.path, download.type);
		}
		pthread_exit((void *)0);
	}

	FILE *p_file = NULL;
	p_file = fopen((const char *)download.path, "rb");
	if (!p_file) {
		FILE_LOG_ERROR("Unable to open download file: %s\n", download.path);
		if(downloadFileCallbacks)
		{
			downloadFileCallbacks(0, -2, download.path, download.type);
		}
		pthread_exit((void *)0);
	}
	long file_size = filesize(p_file);
	fclose(p_file);
	
	if(file_size == 0)
	{
		FILE_LOG_ERROR("File download exception, file size 0.");
		if(downloadFileCallbacks)
		{
			downloadFileCallbacks(0, -3, download.path, download.type);
		}
	}
	else
	{
		FILE_LOG_INFO("File download success, file size %ld", file_size);
		if(downloadFileCallbacks)
		{
			downloadFileCallbacks(0, 0, download.path, download.type);
		}
	}
	
	pthread_exit((void *)0);
}

bool fileDownloadTaskStart(char *url, char *file_path, MESSAGE_TYPE type)
{
	
	FILE_DOWNLOAD_t *event = (FILE_DOWNLOAD_t *)ss_malloc(sizeof(FILE_DOWNLOAD_t));
	memset(event, 0, sizeof(FILE_DOWNLOAD_t));
	
	strcpy(event->url, url);
	strcpy(event->path, file_path);
	event->type = type;
	
	//int *thread_ret = NULL;
	pthread_t ntid;
	int err = pthread_create(&ntid, NULL, pthreadDownload, event);
    //pthread_join( th, (void**)&thread_ret );  
    //printf( "thread_ret = %d.\n", *thread_ret ); 
	pthread_detach(ntid);
	if(err==0) {
		return true;
	} else {
		return false;
	}
}




