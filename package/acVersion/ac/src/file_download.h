#ifndef FILE_DOWNLOAD_H__
#define FILE_DOWNLOAD_H__


#include <stdint.h>
#include <stdbool.h>
#include "com.h"

void fileDownloadCallbackSet(void (*callback)(uint32_t id, int status, char *path, MESSAGE_TYPE type));
bool fileDownloadTaskStart(char *url, char *file_path, MESSAGE_TYPE type);



#endif //FILE_DOWNLOAD_H__

