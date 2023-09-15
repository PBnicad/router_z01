#include "upgrade_serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/stat.h>   
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#include "easy_zip_parse.h"
#include "crc32.h"

#define UPGRADE_LOG_INFO(...)  //printf(__VA_ARGS__)

UPGRADE_DFU_t upgradeDfuConsole = {0};
UPGRADE_FILE_t selectUpgradeFile = {0};

uint32_t uint32_decode(uint8_t *p_encoded_data, uint32_t index)
{
    return ( (((uint32_t)p_encoded_data[index+0]) << 0)  |
             (((uint32_t)p_encoded_data[index+1]) << 8)  |
             (((uint32_t)p_encoded_data[index+2]) << 16) |
             (((uint32_t)p_encoded_data[index+3]) << 24 ));
}


typedef struct {
	uint8_t *data;
	uint16_t size;
}OBJECT_PACK_t;

OBJECT_PACK_t *cmdQueryCmdObject(void){
	static uint8_t data[2] = {NRF_DFU_OP_OBJECT_SELECT, NRF_DFU_OBJ_TYPE_COMMAND};
	static OBJECT_PACK_t cmd = {data, 2};
	return &cmd;
}
OBJECT_PACK_t *cmdQueryDataObject(void){
	static uint8_t data[2] = {NRF_DFU_OP_OBJECT_SELECT, NRF_DFU_OBJ_TYPE_DATA};
	static OBJECT_PACK_t cmd = {data, 2};
	return &cmd;
}
OBJECT_PACK_t *cmdNotifyDisable(void){
	static uint8_t data[3] = {NRF_DFU_OP_RECEIPT_NOTIF_SET, 0, 0};
	static OBJECT_PACK_t cmd = {data, 3};
	return &cmd;
}
OBJECT_PACK_t *cmdNotifyEnable(void){
	static uint8_t data[3] = {NRF_DFU_OP_RECEIPT_NOTIF_SET, 0x50, 0};
	static OBJECT_PACK_t cmd = {data, 3};
	return &cmd;
}
OBJECT_PACK_t *cmdCrcGet(void){
	static uint8_t data[1] = {NRF_DFU_OP_CRC_GET};
	static OBJECT_PACK_t cmd = {data, 1};
	return &cmd;
}
OBJECT_PACK_t *cmdObjectExec(void){
	static uint8_t data[1] = {NRF_DFU_OP_OBJECT_EXECUTE};
	static OBJECT_PACK_t cmd = {data, 1};
	return &cmd;
}
OBJECT_PACK_t *cmdQueryMTUSize(void){
	static uint8_t data[1] = {NRF_DFU_OP_MTU_GET};
	static OBJECT_PACK_t cmd = {data, 1};
	return &cmd;
}
OBJECT_PACK_t *cmdPing(uint8_t id)
{
	static uint8_t data[2] = {NRF_DFU_OP_PING};
	data[1] = id;
	static OBJECT_PACK_t cmd = {data, 1};
	return &cmd;
}
OBJECT_PACK_t *cmdCreateCmd(uint32_t cmdSize) {
	static uint8_t data[6] = {NRF_DFU_OP_OBJECT_CREATE, NRF_DFU_OBJ_TYPE_COMMAND};
	data[2] = (cmdSize>>0) & 0xFF;
	data[3] = (cmdSize>>8) & 0xFF;
	data[4] = (cmdSize>>16) & 0xFF;
	data[5] = (cmdSize>>24) & 0xFF;
	static OBJECT_PACK_t cmd = {data, 6};
	return &cmd;
}
OBJECT_PACK_t *cmdCreateData(uint32_t dataSize) {
	static uint8_t data[6] = {NRF_DFU_OP_OBJECT_CREATE, NRF_DFU_OBJ_TYPE_DATA};
	data[2] = (dataSize) & 0xFF;
	data[3] = (dataSize>>8) & 0xFF;
	data[4] = (dataSize>>16) & 0xFF;
	data[5] = (dataSize>>24) & 0xFF;
	static OBJECT_PACK_t cmd = {data, 6};
	return &cmd;
}

void (*responed_func)(uint8_t *data, uint32_t length, bool encode);
void (*serial_buffer_reset_func)(void);
	
/**
 * @brief upgradeWriteCmd
 * 写命令
 * @param device
 * @param cmd
 */
void upgradeWriteCmd(UPGRADE_DFU_t *device, OBJECT_PACK_t *cmd)
{
	UPGRADE_LOG_INFO("write cmd size %d:\n", cmd->size);
	for(int i=0; i<cmd->size; i++) {
		UPGRADE_LOG_INFO("%02X ", cmd->data[i]);
	}
	UPGRADE_LOG_INFO("\n");
	
	if(!responed_func) {
		UPGRADE_LOG_INFO("serial responed handler is NULL, can't write data.\n");
		return;
	}
	
	responed_func(cmd->data, cmd->size, true);
}

/**
 * @brief upgradeWriteData
 * 写数据
 * @param device
 * @param data
 */
void upgradeWriteData(UPGRADE_DFU_t *device, OBJECT_PACK_t *data)
{
	uint8_t write_buf[1024] = {0};
	
	uint32_t pack_size = data->size;
	uint8_t *p_data = data->data;
	uint16_t mtu_esc_size = (device->object_mtu_size - 1)/2;
	
	if(mtu_esc_size%4!=0) {
		mtu_esc_size = mtu_esc_size/4*4;
	}
	
	UPGRADE_LOG_INFO("start write data size: %d\n", data->size);
	if(!responed_func) {
		UPGRADE_LOG_INFO("serial responed handler is NULL, can't write data.\n");
		return;
	}
	
	while(pack_size >= mtu_esc_size) {
		
		memset(write_buf, 0, sizeof(write_buf));
		memcpy(&write_buf[1], p_data, mtu_esc_size);
		write_buf[0] = NRF_DFU_OP_OBJECT_WRITE;
		responed_func(write_buf, mtu_esc_size+1, true);
		
		p_data += mtu_esc_size;
		pack_size -= mtu_esc_size;
	}
	if(pack_size) {
		
		memset(write_buf, 0, sizeof(write_buf));
		memcpy(&write_buf[1], p_data, pack_size);
		write_buf[0] = NRF_DFU_OP_OBJECT_WRITE;
		responed_func(write_buf, pack_size+1, true);
	}
}




/**
 * @brief upgradeOpExec
 * 根据设备响应执行下一步动作
 * @param newDevice
 */
static void upgradeOpExec(UPGRADE_DFU_t *newDevice)
{
    switch(newDevice->op_code)
    {
		case DFU_OP_RSQ_PING:
            upgradeWriteCmd(newDevice, cmdPing(123));
            break;
		case DFU_OP_QUERY_MTU_SIZE:
            upgradeWriteCmd(newDevice, cmdQueryMTUSize());
            break;
        case DFU_OP_QUERY_CMD_OBJECT:   //
            //nrf_dfu_ext_ctrl_pt_query_cmd_object();
            upgradeWriteCmd(newDevice, cmdQueryCmdObject());
            break;
        case DFU_OP_NOTIFY_DISABLE:     //
            newDevice->send_crc = 0;
            //nrf_dfu_ext_ctrl_pt_notify_disable();
            upgradeWriteCmd(newDevice, cmdNotifyDisable());
            break;
        case DFU_OP_RSQ_SEND_CMD:       //
            //nrf_dfu_ext_ctrl_pt_create_cmd(newDevice->setting_size);
            upgradeWriteCmd(newDevice, cmdCreateCmd(selectUpgradeFile.datSize));
            break;
        case DFU_OP_SEND_CMD_OBJECT:    //
        {
            uint8_t setting_info[200] = { 0 };
			FILE *file = fopen(selectUpgradeFile.fileName, "rb");
			
            if(file) {
				fseek(file, selectUpgradeFile.datOffset, SEEK_SET);
				fread(setting_info, 1, selectUpgradeFile.datSize, file);
                fclose(file);
            } else {

            }
            //nrf_dfu_ext_data_read(newDevice->setting_soucre, newDevice->setting_size, setting_info);
            newDevice->send_crc = crc32_compute(setting_info, selectUpgradeFile.datSize, NULL);
            //nrf_dfu_ext_write(setting_info, newDevice->setting_size);
			OBJECT_PACK_t pkg = {0};
			pkg.data = setting_info;
			pkg.size = selectUpgradeFile.datSize;
            upgradeWriteData(newDevice, &pkg);
            /*发送完后请求校验码*/
            newDevice->op_code = DFU_OP_RSQ_OFCRC;
            //upgradeOpExec(newDevice);
            upgradeOpExec(newDevice);
        }
            break;
        case DFU_OP_RSQ_OFCRC:          //
        case DFU_OP_RSQ_DATA_OFCRC:     //
            //nrf_dfu_ext_ctrl_pt_crc_get();
            upgradeWriteCmd(newDevice, cmdCrcGet());
            break;
        case DFU_OP_EXEC_CMD:           //
        case DFU_OP_EXEC_DATA:          //
            //nrf_dfu_ext_ctrl_pt_crc_exec();
            upgradeWriteCmd(newDevice, cmdObjectExec());
            break;

        case DFU_OP_NOTIFY_ENABLE:      //
            newDevice->send_crc = 0;
            //nrf_dfu_ext_ctrl_pt_notify_enable();
            upgradeWriteCmd(newDevice, cmdNotifyEnable());
            break;
        case DFU_OP_QUERY_DATA_OBJECT:  //
            //nrf_dfu_ext_ctrl_pt_query_data_object();
            upgradeWriteCmd(newDevice, cmdQueryDataObject());
            break;

        case DFU_OP_RSQ_SEND_DATA:      //
            if(selectUpgradeFile.binSize - newDevice->firmware_offset
                > newDevice->object_data_size)
            {
                //nrf_dfu_ext_ctrl_pt_create_data(newDevice->object_data_size);
                upgradeWriteCmd(newDevice, cmdCreateData(newDevice->object_data_size));
            }
            else
            {
                //nrf_dfu_ext_ctrl_pt_create_data(selectUpgradeFile.binSize - newDevice->firmware_offset);
                upgradeWriteCmd(newDevice,
                                cmdCreateData(selectUpgradeFile.binSize - newDevice->firmware_offset));
            }
            break;

        case DFU_OP_SEND_DATA_OBJECT:   //
        {
            unsigned char buf[4096] = {0};
            if(selectUpgradeFile.binSize - newDevice->firmware_offset
                > newDevice->object_data_size)
            {
				FILE *file = fopen(selectUpgradeFile.fileName, "rb");
				
				if(file) {
					fseek(file, selectUpgradeFile.binOffset + newDevice->firmware_offset, SEEK_SET);
					fread(buf, 1, newDevice->object_data_size, file);
					fclose(file);
				} else {
					//error
					UPGRADE_LOG_INFO("get error rsp");
				}
				
//				nrf_dfu_ext_data_read(newDevice->firmware_soucre + newDevice->firmware_offset,
//										newDevice->object_data_size,
//										buf);
                newDevice->send_crc = crc32_compute(buf, newDevice->object_data_size, &newDevice->send_crc);
//				nrf_dfu_ext_write(buf, newDevice->object_data_size);
				OBJECT_PACK_t pkg = {0};
				pkg.data = buf;
				pkg.size = newDevice->object_data_size;
                upgradeWriteData(newDevice, &pkg);

                newDevice->firmware_offset += newDevice->object_data_size;
            }
            else
            {
				FILE *file = fopen(selectUpgradeFile.fileName, "rb");
				if(file) {
					fseek(file, selectUpgradeFile.binOffset + newDevice->firmware_offset, SEEK_SET);
					fread(buf, 1, selectUpgradeFile.binSize - newDevice->firmware_offset, file);
					fclose(file);
				} else {
					//error
					UPGRADE_LOG_INFO("get error rsp");
				}

//                nrf_dfu_ext_data_read(newDevice->firmware_soucre + newDevice->firmware_offset,
//                                        selectUpgradeFile.binSize - newDevice->firmware_offset,
//                                        buf);
                newDevice->send_crc = crc32_compute(buf, selectUpgradeFile.binSize - newDevice->firmware_offset,
                                                        &newDevice->send_crc);
//                nrf_dfu_ext_write(buf, selectUpgradeFile.binSize - newDevice->firmware_offset);
				OBJECT_PACK_t pkg = {0};
				pkg.data = buf;
				pkg.size = selectUpgradeFile.binSize - newDevice->firmware_offset;
                upgradeWriteData(newDevice, &pkg);
				
                newDevice->firmware_offset += selectUpgradeFile.binSize - newDevice->firmware_offset;
            }

            /*发送完后请求校验码*/
            newDevice->op_code = DFU_OP_RSQ_DATA_OFCRC;
            //upgradeOpExec(newDevice);
            upgradeOpExec(newDevice);
        }
        break;
    }
}

static void upgradeError(uint8_t *err_data)
{
	if(err_data[0]==NRF_DFU_RES_CODE_EXT_ERROR) {
		upgradeDfuConsole.err_code[0] = err_data[0];
		upgradeDfuConsole.err_code[1] = err_data[1];
	} else {
		upgradeDfuConsole.err_code[0] = err_data[0];
	}
	switch(err_data[0]) {
		case NRF_DFU_RES_CODE_INVALID: UPGRADE_LOG_INFO("Invalid opcode.\n");
			break;
		case NRF_DFU_RES_CODE_SUCCESS: UPGRADE_LOG_INFO("Operation successful.\n");
			break;
		case NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED: UPGRADE_LOG_INFO("Opcode not supported.\n");
			break;
		case NRF_DFU_RES_CODE_INVALID_PARAMETER: UPGRADE_LOG_INFO("Missing or invalid parameter value.\n");
			break;
		case NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES: UPGRADE_LOG_INFO("Not enough memory for the data object.\n");
			break;
		case NRF_DFU_RES_CODE_INVALID_OBJECT: UPGRADE_LOG_INFO("Data object does not match the firmware and hardware requirements,"
													 " the signature is wrong, or parsing the command failed.\n");
			break;
		case NRF_DFU_RES_CODE_UNSUPPORTED_TYPE: UPGRADE_LOG_INFO("Not a valid object type for a Create request.\n");
			break;
		case NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED: UPGRADE_LOG_INFO("The state of the DFU process does not allow this operation.\n");
			break;
		case NRF_DFU_RES_CODE_OPERATION_FAILED: UPGRADE_LOG_INFO("Operation failed.\n");
			break;
	}

	upgradeDfuConsole.op_code = DFU_OP_COMPLETE;
	
	if(err_data[0] != NRF_DFU_RES_CODE_EXT_ERROR)
		return;
	
	//Extended error. The next byte of the response contains the error code of the extended error (see @ref nrf_dfu_ext_error_code_t.
	
	switch(err_data[1]) {
		case NRF_DFU_EXT_ERROR_NO_ERROR: UPGRADE_LOG_INFO("No extended error code has been set. This error indicates an implementation problem.\n");
			break;
		case NRF_DFU_EXT_ERROR_INVALID_ERROR_CODE: UPGRADE_LOG_INFO("Invalid error code. This error code should never be used outside of development.\n");
			break;
		case NRF_DFU_EXT_ERROR_WRONG_COMMAND_FORMAT: UPGRADE_LOG_INFO("The format of the command was incorrect. This error code is not used in the"
																 "current implementation, because @ref NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED "
																 "and @ref NRF_DFU_RES_CODE_INVALID_PARAMETER cover all "
																 "possible format errors.\n");
			break;
		case NRF_DFU_EXT_ERROR_UNKNOWN_COMMAND: UPGRADE_LOG_INFO("The command was successfully parsed, but it is not supported or unknown.\n");
			break;
		case NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID: UPGRADE_LOG_INFO("The init command is invalid. The init packet either has "
																 "an invalid update type or it is missing required fields for the update type"
																 "(for example, the init packet for a SoftDevice update is missing the SoftDevice size field).\n");
			break;
		case NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE: UPGRADE_LOG_INFO("The firmware version is too low. For an application, the version must be greater than"
																" the current application. For a bootloader, it must be greater than or equal"
																" to the current version. This requirement prevents downgrade attacks.\n");
			break;
		case NRF_DFU_EXT_ERROR_HW_VERSION_FAILURE: UPGRADE_LOG_INFO("The hardware version of the device does not match the required"
																" hardware version for the update.\n");
			break;
		case NRF_DFU_EXT_ERROR_SD_VERSION_FAILURE: UPGRADE_LOG_INFO("The array of supported SoftDevices for the update does not contain"
																" the FWID of the current SoftDevice or the first FWID is '0' on a"
																" bootloader which requires the SoftDevice to be present.\n");
			break;
		case NRF_DFU_EXT_ERROR_SIGNATURE_MISSING: UPGRADE_LOG_INFO("The init packet does not contain a signature. This error code is not used in the"
																" current implementation, because init packets without a signature"
																" are regarded as invalid.\n");
			break;
		case NRF_DFU_EXT_ERROR_WRONG_HASH_TYPE: UPGRADE_LOG_INFO("The hash type that is specified by the init packet is not supported by the DFU bootloader.\n");
			break;
		case NRF_DFU_EXT_ERROR_HASH_FAILED: UPGRADE_LOG_INFO("The hash of the firmware image cannot be calculated.\n");
			break;
		case NRF_DFU_EXT_ERROR_WRONG_SIGNATURE_TYPE: UPGRADE_LOG_INFO("The type of the signature is unknown or not supported by the DFU bootloader.\n");
			break;
		case NRF_DFU_EXT_ERROR_VERIFICATION_FAILED: UPGRADE_LOG_INFO("The hash of the received firmware image does not match the hash in the init packet.\n");
			break;
		case NRF_DFU_EXT_ERROR_INSUFFICIENT_SPACE: UPGRADE_LOG_INFO("The available space on the device is insufficient to hold the firmware.\n");
			break;
	}
}

bool upgradeSerialUartRecv(uint8_t *data, uint32_t size)
{
	//emit upgardeDebug(newDevice->info.name(),data.toHex());
	UPGRADE_DFU_t *newDevice = &upgradeDfuConsole;
	
	UPGRADE_LOG_INFO("upgradeSerialUartRecv:\n");
	for(int i=0; i<size; i++) {
		UPGRADE_LOG_INFO("%02X ", data[i]);
	}
	UPGRADE_LOG_INFO("\n");

	switch(data[1])
	{
		case NRF_DFU_OP_OBJECT_CREATE: //0x01 create object

			if(data[2]==NRF_DFU_RES_CODE_SUCCESS) {
				newDevice->op_code++; //设置才进行下一步, 否则重复操作
			} else {
				upgradeError(&data[2]);
				return true;
			}
			upgradeOpExec(newDevice);
		break;
		case NRF_DFU_OP_RECEIPT_NOTIF_SET:  // 0x02 notify set
		{
			/*
				Number of packets of firmware data received after sending last Packet Receipt
				Notification or since the receipt of a @ref BLE_DFU_PKT_RCPT_NOTIF_ENABLED
				event from the DFU service, which ever occurs later
			*/
			if(data[2]==NRF_DFU_RES_CODE_SUCCESS) {
				newDevice->op_code++; //设置才进行下一步, 否则重复操作
			} else {
				upgradeError(&data[2]);
				return true;
			}
			upgradeOpExec(newDevice);
		}
		break;
		case NRF_DFU_OP_CRC_GET:       //0x03 crc
		{
			uint32_t offset = uint32_decode(data, 3);
			newDevice->response_crc = uint32_decode(data, 3+4);

			UPGRADE_LOG_INFO("%d %d %d\n", newDevice->firmware_offset, offset, selectUpgradeFile.binSize);

			if(newDevice->op_code == DFU_OP_RSQ_DATA_OFCRC)
			{
				newDevice->firmware_offset = offset;
			}

			if(newDevice->response_crc == newDevice->send_crc)
			{
				newDevice->op_code++;
				upgradeOpExec(newDevice);
			} else {
				UPGRADE_LOG_INFO("crc: %u ?= %u\n", newDevice->response_crc, newDevice->send_crc);
				uint8_t err = 0xff;
				upgradeError(&err);
				return true;
			}
		}
		break;
		case NRF_DFU_OP_OBJECT_EXECUTE: //0x04
		{
			//emit upgardeDebug(newDevice->info.name(),"NRF_DFU_OP_OBJECT_EXECUTE");
			if(data[2] == NRF_DFU_RES_CODE_SUCCESS) {
				if(newDevice->op_code == DFU_OP_EXEC_CMD)
				{
					//emit upgardeDebug(newDevice->info.name(),"DFU_OP_EXEC_CMD");
					newDevice->op_code++;
					upgradeOpExec(newDevice);
				}
				else if(newDevice->op_code == DFU_OP_EXEC_DATA)
				{
					//emit upgardeDebug(newDevice->info.name(),"DFU_OP_EXEC_DATA");


					if(selectUpgradeFile.binSize != newDevice->firmware_offset)
					{
						newDevice->op_code = DFU_OP_RSQ_SEND_DATA;
						upgradeOpExec(newDevice);
					}
					else
					{
						//升级完成
						
						struct timeval tv;
						gettimeofday(&tv, NULL);
						uint32_t transfer_time = tv.tv_sec - newDevice->timeStart;
						
						transfer_time *= 1000;
						transfer_time += tv.tv_usec/1000;
						transfer_time -= newDevice->timeStartUs/1000;
						
						newDevice->err_code[0] = 0xF0;
						newDevice->timeTransfer = transfer_time;
						newDevice->op_code = DFU_OP_COMPLETE;
						
						
						UPGRADE_LOG_INFO("upgrade transfer complete, used time %d ms\n", transfer_time);
						
						return true;
					}
				}
			} else {
				 //upgradeDfuErrorInfo(newDevice->info.name(),data.mid(2));
				UPGRADE_LOG_INFO("NRF_DFU_OP_OBJECT_EXECUTE error %d\n", data[2]);
				upgradeError(&data[2]);
				return true;
			}
		}
		break;
		case NRF_DFU_OP_OBJECT_SELECT: //0x06 object select
			//emit upgardeDebug(newDevice->info.name(),"NRF_DFU_OP_OBJECT_SELECT");
			if(data[2] == NRF_DFU_RES_CODE_SUCCESS)
			{
				if(newDevice->op_code == DFU_OP_QUERY_CMD_OBJECT)
				{
					newDevice->object_cmd_size = uint32_decode(data, 3);
					newDevice->op_code++;
					UPGRADE_LOG_INFO("query object_cmd_size %d\n", newDevice->object_cmd_size);
					upgradeOpExec(newDevice);
				}
				else if(newDevice->op_code == DFU_OP_QUERY_DATA_OBJECT)
				{
					//emit upgardeDebug(newDevice->info.name(),"DFU_OP_QUERY_DATA_OBJECT");
					newDevice->object_data_size = uint32_decode(data, 3);
					UPGRADE_LOG_INFO("query object_data_size %d\n", newDevice->object_data_size);
					newDevice->firmware_offset = uint32_decode(data, 3+4);
					newDevice->send_crc = uint32_decode(data, 3+8);
					newDevice->op_code++;
					upgradeOpExec(newDevice);
				}
			} else {
				UPGRADE_LOG_INFO("NRF_DFU_OP_OBJECT_SELECT error %d\n", data[2]);
				//upgradeDfuErrorInfo(newDevice->info.name(),data.mid(2));
				upgradeError(&data[2]);
				return true;
			}
		break;
		case NRF_DFU_OP_MTU_GET:
			if(data[2] == NRF_DFU_RES_CODE_SUCCESS) {
				newDevice->object_mtu_size = data[3] | data[4]<<8;
				UPGRADE_LOG_INFO("query mtu size: %d\n", newDevice->object_mtu_size);
				newDevice->op_code++;
				upgradeOpExec(newDevice);
			} else {
				UPGRADE_LOG_INFO("NRF_DFU_OP_MTU_GET error %d", data[2]);
				//upgradeDfuErrorInfo(newDevice->info.name(),data.mid(2));
				upgradeError(&data[2]);
				return true;
			}
			
		break;
		case NRF_DFU_OP_OBJECT_WRITE:
		
		break;
		case NRF_DFU_OP_PING:
		{
			struct timeval tv;
			gettimeofday(&tv, NULL);
			newDevice->timeStart =  tv.tv_sec;
			newDevice->timeStartUs = tv.tv_usec;
			newDevice->op_code++;
			upgradeOpExec(newDevice);
		}
		break;
		default:
			break;
	}
	
	return false;
}


bool upgradeOnPingStatus(void)
{
	if(upgradeDfuConsole.op_code == DFU_OP_RSQ_PING)
		return true;
	return false;
}


bool upgradeIsComplete(void)
{
	if(upgradeDfuConsole.op_code == DFU_OP_COMPLETE)
		return true;
	return false;
}

bool upgradeIsIdle(void)
{
	if(upgradeDfuConsole.op_code == DFU_OP_IDLE)
		return true;
	return false;
}

void upgradeClean(void)
{
	memset(&upgradeDfuConsole, 0, sizeof(upgradeDfuConsole));
	memset(&selectUpgradeFile, 0, sizeof(selectUpgradeFile));
}

uint8_t *upgradeGetError(void)
{
	return upgradeDfuConsole.err_code;
}

uint8_t upgradeProgressPercent(void)
{
	uint32_t per = selectUpgradeFile.binSize/100;
	return upgradeDfuConsole.firmware_offset/per;
}

void responedFuncRegsiter(void (*rsp_func)(uint8_t *data, uint32_t length, bool endcode))
{
	responed_func = rsp_func;
}

void upgradeSerialResetFuncRegsiter(void (*reset_func)(void))
{
	serial_buffer_reset_func = reset_func;
}


void upgradeStart(char *upgrade_file)
{
	memset(&upgradeDfuConsole, 0, sizeof(upgradeDfuConsole));
	memset(&selectUpgradeFile, 0, sizeof(selectUpgradeFile));
	
	easyZipParse(upgrade_file, &selectUpgradeFile);
	
	if(serial_buffer_reset_func) {
		serial_buffer_reset_func();
	}
	/* 开始升级,ping检测 */
	upgradeDfuConsole.op_code = DFU_OP_RSQ_PING;
	upgradeOpExec(&upgradeDfuConsole);

}

void upgradeRestart(void)
{
	/* 开始升级,ping检测 */
	upgradeDfuConsole.op_code = DFU_OP_RSQ_PING;
	upgradeOpExec(&upgradeDfuConsole);
}

void upgardeDfuEnter(void)
{
	unsigned char data[] = { 0xAA,0x8C,0xCA,0x05,0x5C,0x00,0x01,0x17,0xA0,0xD7,
							0xCC,0xCB,0xCA,0xC9,0xC8,0x00,0x00,0xe2,0x07,0x55 };
						
	if(serial_buffer_reset_func) {
		serial_buffer_reset_func();
	}	
	if(responed_func) {
		responed_func(data, sizeof(data), false);
		/*wire twice*/
		responed_func(data, sizeof(data), false);
	}
	UPGRADE_LOG_INFO("contrl enter dfu mode\n");
	sleep(1);
}

