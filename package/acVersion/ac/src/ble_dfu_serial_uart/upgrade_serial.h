#ifndef UPGRADE_SERIAL_H__
#define UPGRADE_SERIAL_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    NRF_DFU_EXT_ERROR_NO_ERROR                  = 0x00, /**< No extended error code has been set. This error indicates an implementation problem. */
    NRF_DFU_EXT_ERROR_INVALID_ERROR_CODE        = 0x01, /**< Invalid error code. This error code should never be used outside of development. */
    NRF_DFU_EXT_ERROR_WRONG_COMMAND_FORMAT      = 0x02, /**< The format of the command was incorrect. This error code is not used in the
                                                             current implementation, because @ref NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED
                                                             and @ref NRF_DFU_RES_CODE_INVALID_PARAMETER cover all
                                                             possible format errors. */
    NRF_DFU_EXT_ERROR_UNKNOWN_COMMAND           = 0x03, /**< The command was successfully parsed, but it is not supported or unknown. */
    NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID      = 0x04, /**< The init command is invalid. The init packet either has
                                                             an invalid update type or it is missing required fields for the update type
                                                             (for example, the init packet for a SoftDevice update is missing the SoftDevice size field). */
    NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE        = 0x05, /**< The firmware version is too low. For an application, the version must be greater than
                                                             the current application. For a bootloader, it must be greater than or equal
                                                             to the current version. This requirement prevents downgrade attacks.*/
    NRF_DFU_EXT_ERROR_HW_VERSION_FAILURE        = 0x06, /**< The hardware version of the device does not match the required
                                                             hardware version for the update. */
    NRF_DFU_EXT_ERROR_SD_VERSION_FAILURE        = 0x07, /**< The array of supported SoftDevices for the update does not contain
                                                             the FWID of the current SoftDevice or the first FWID is '0' on a
                                                             bootloader which requires the SoftDevice to be present. */
    NRF_DFU_EXT_ERROR_SIGNATURE_MISSING         = 0x08, /**< The init packet does not contain a signature. This error code is not used in the
                                                             current implementation, because init packets without a signature
                                                             are regarded as invalid. */
    NRF_DFU_EXT_ERROR_WRONG_HASH_TYPE           = 0x09, /**< The hash type that is specified by the init packet is not supported by the DFU bootloader. */
    NRF_DFU_EXT_ERROR_HASH_FAILED               = 0x0A, /**< The hash of the firmware image cannot be calculated. */
    NRF_DFU_EXT_ERROR_WRONG_SIGNATURE_TYPE      = 0x0B, /**< The type of the signature is unknown or not supported by the DFU bootloader. */
    NRF_DFU_EXT_ERROR_VERIFICATION_FAILED       = 0x0C, /**< The hash of the received firmware image does not match the hash in the init packet. */
    NRF_DFU_EXT_ERROR_INSUFFICIENT_SPACE        = 0x0D, /**< The available space on the device is insufficient to hold the firmware. */
} nrf_dfu_ext_error_code_t;

typedef enum
{
    NRF_DFU_OBJ_TYPE_INVALID,                   //!< Invalid object type.
    NRF_DFU_OBJ_TYPE_COMMAND,                   //!< Command object.
    NRF_DFU_OBJ_TYPE_DATA,                      //!< Data object.
} nrf_dfu_obj_type_t;

typedef enum
{
    NRF_DFU_RES_CODE_INVALID                 = 0x00,    //!< Invalid opcode.
    NRF_DFU_RES_CODE_SUCCESS                 = 0x01,    //!< Operation successful.
    NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED   = 0x02,    //!< Opcode not supported.
    NRF_DFU_RES_CODE_INVALID_PARAMETER       = 0x03,    //!< Missing or invalid parameter value.
    NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES  = 0x04,    //!< Not enough memory for the data object.
    NRF_DFU_RES_CODE_INVALID_OBJECT          = 0x05,    //!< Data object does not match the firmware and hardware requirements, the signature is wrong, or parsing the command failed.
    NRF_DFU_RES_CODE_UNSUPPORTED_TYPE        = 0x07,    //!< Not a valid object type for a Create request.
    NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED = 0x08,    //!< The state of the DFU process does not allow this operation.
    NRF_DFU_RES_CODE_OPERATION_FAILED        = 0x0A,    //!< Operation failed.
    NRF_DFU_RES_CODE_EXT_ERROR               = 0x0B,    //!< Extended error. The next byte of the response contains the error code of the extended error (see @ref nrf_dfu_ext_error_code_t.
} nrf_dfu_result_t;

typedef enum
{
    NRF_DFU_OP_PROTOCOL_VERSION     = 0x00,     //!< Retrieve protocol version.
    NRF_DFU_OP_OBJECT_CREATE        = 0x01,     //!< Create selected object.
    NRF_DFU_OP_RECEIPT_NOTIF_SET    = 0x02,     //!< Set receipt notification.
    NRF_DFU_OP_CRC_GET              = 0x03,     //!< Request CRC of selected object.
    NRF_DFU_OP_OBJECT_EXECUTE       = 0x04,     //!< Execute selected object.
    NRF_DFU_OP_OBJECT_SELECT        = 0x06,     //!< Select object.
    NRF_DFU_OP_MTU_GET              = 0x07,     //!< Retrieve MTU size.
    NRF_DFU_OP_OBJECT_WRITE         = 0x08,     //!< Write selected object.
    NRF_DFU_OP_PING                 = 0x09,     //!< Ping.
    NRF_DFU_OP_HARDWARE_VERSION     = 0x0A,     //!< Retrieve hardware version.
    NRF_DFU_OP_FIRMWARE_VERSION     = 0x0B,     //!< Retrieve firmware version.
    NRF_DFU_OP_ABORT                = 0x0C,     //!< Abort the DFU procedure.
    NRF_DFU_OP_RESPONSE             = 0x60,     //!< Response.
    NRF_DFU_OP_INVALID              = 0xFF,
} nrf_dfu_op_t;


/*
升级流程分两大步骤:
1.发送初始化数据
2.发送固件
*/

typedef enum {
	DFU_OP_IDLE,
	DFU_OP_RSQ_PING,           //ping检查串口通畅
	DFU_OP_NOTIFY_DISABLE,     //设置PRN
	DFU_OP_QUERY_MTU_SIZE,     //查询MTU大小
	
    DFU_OP_QUERY_CMD_OBJECT,   //查询命令空间大小并记录
	
    DFU_OP_RSQ_SEND_CMD,       //
    DFU_OP_SEND_CMD_OBJECT,    //
    DFU_OP_RSQ_OFCRC,          //
    DFU_OP_EXEC_CMD,           //这步执行后根据响判断是否允许升级 响应码参考 nrf_dfu_result_t

    DFU_OP_NOTIFY_ENABLE,      //发送数据前打开notify
    DFU_OP_QUERY_DATA_OBJECT,  //查询数据空间大小并记录

    DFU_OP_RSQ_SEND_DATA,      //根据数据空间大小分包发送,直至发送完毕
    DFU_OP_SEND_DATA_OBJECT,   //
    DFU_OP_RSQ_DATA_OFCRC,     //
    DFU_OP_EXEC_DATA,          //11
	
	DFU_OP_COMPLETE,
}BLE_OP_DFU;


typedef struct
{
	
    uint32_t op_code;           //操作步骤,流程步骤

    uint32_t send_crc;          //发送数据的连续CRC
    uint32_t response_crc;      //响应的连续CRC

    uint32_t object_cmd_size;   //命令包空间大小
    uint32_t object_data_size;  //数据包空间大小
	uint16_t object_mtu_size;   //串口数据包空间大小

    uint32_t firmware_offset;   //偏移
    uint16_t firmware_pack_ind; //包序号

    uint32_t timeStart;
	uint32_t timeStartUs;
    uint32_t timeTransfer;
	
	uint8_t err_code[2];
}UPGRADE_DFU_t;

void upgradeSerialResetFuncRegsiter(void (*reset_func)(void));
void responedFuncRegsiter(void (*rsp_func)(uint8_t *data, uint32_t length, bool endcode));
bool upgradeSerialUartRecv(uint8_t *data, uint32_t size);


void upgradeStart(char *upgrade_file);
void upgradeRestart(void);
bool upgradeOnPingStatus(void);
bool upgradeIsComplete(void);
bool upgradeIsIdle(void);
void upgradeClean(void);
void upgardeDfuEnter(void);
uint8_t *upgradeGetError(void);
uint8_t upgradeProgressPercent(void);

#endif //UPGRADE_SERIAL_H__


