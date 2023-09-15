/************************************************************************************* 
filename:     	transcoder.c
create:       	2019.11.20 LUJUNLIN
revised:     	
revision:     	1.0
description:	Convert hexadecimal data transfer to json data transfer
**************************************************************************************/


#ifndef _TRANSCODER_H
#define _TRANSCODER_H


#include <stdbool.h>
#include <json-c/json.h>
#include <syslog.h>
#include "guart.h"
#include "com.h"

//#define	DEBUG	1
extern bool DEBUG;



/*****************************************************************************************************
@function:      void transDebug(uint8_t *data, uint16_t len)
@description:  	Outputs hexadecimal data to the log
@param:			data	:hex data
				len		:hex data len
@return:        none
******************************************************************************************************/
void transDebug(uint8_t *data, uint16_t len);


/*****************************************************************************************************
@function:      void transWifiInfo(uint8_t * toBuff)
@description:   Generate wifi information
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transWifiInfo(uint8_t * toBuff);


/*****************************************************************************************************
@function:      void transRouterInfo(uint8_t *toBuff)
@description:   Generate routing information
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transRouterInfo(uint8_t *toBuff);


/*****************************************************************************************************
@function:      void transNetworkStatistics(uint8_t * toBuff)
@description:   Network Statistics 
@param:         toBuff: Output json data
@return:        none
******************************************************************************************************/
extern void transNetworkStatistics(uint8_t * toBuff);


/*****************************************************************************************************
@function:      extern uint16_t transToJson(uint8_t *send_sock_buf, uint32_t send_sock_buf_len, 
					uint8_t *toBuff, uint16_t tobuff_max_len)
@description:   Convert hexadecimal data to json data
@param:			send_sock_buf		:hexadecimal data
				send_sock_buf_len	:hexadecimal data len
				toBuff				:output json string data
				tobuff_max_len		:toBuff max size, check array index out of bounds
@return:        toBuff lenght
******************************************************************************************************/
extern uint16_t transToJson(uint8_t *send_sock_buf, uint32_t send_sock_buf_len, uint8_t *toBuff, uint16_t tobuff_max_len);


/*****************************************************************************************************
@function:      extern uint16_t transJsonToHex(uint8_t *json_data, uint32_t json_data_len, 
					uint8_t *toBuff, sock_ctx_t *sock_ctx)
@description:   Convert json data to hexadecimal data
@param:         json_data:		input data,Json format string;
				json_data_len:	input data lenght;
				toBuff:			output data buff;
				sock_ctx:		Main transfer file descriptor used to send data
@return:        toBuff lenght
******************************************************************************************************/
extern uint16_t transJsonToHex(uint8_t *json_data, uint32_t json_data_len, uint8_t *toBuff, struct mosquitto *mosq);


/*****************************************************************************************************
@function:      void transRecvDownData(uint8_t *data, uint16_t data_len, sock_ctx_t *sock_ctx)
@description:  	processing data, The data is spliced and split into json data
@param:         data:		from data.  
				data_len:	from data lengt.   
				sock_ctx:	struct, Primary transfer file descriptor
@return:        none
******************************************************************************************************/
extern void transRecvDownData(uint8_t *data, uint16_t data_len, struct mosquitto *mosq);


#endif



