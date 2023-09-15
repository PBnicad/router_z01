#ifndef GJSON_MESSAGE_H__
#define GJSON_MESSAGE_H__

#include <mosquitto.h>

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>

#include "com.h"

extern void transRouterInfo(uint8_t *toBuff);


void gjsonMessageCallback(uint8_t *json_buf, uint16_t json_buff_len, MESSAGE_TYPE type);






#endif //GJSON_MESSAGE_H__
