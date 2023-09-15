#include "easy_zip_parse.h"
#include "crc32.h"
#include <string.h>

#define ZIP_LOG_INFO(...) printf(__VA_ARGS__)

#pragma pack(2)
/* zip升级包文件头信息 */
typedef struct {
    uint32_t headFlag;
    uint16_t pkware;
    uint16_t flag;
    uint16_t compressMode;
    uint16_t lastModifyTime;
    uint16_t lastModifyDate;
    uint32_t crc32;
    uint32_t compressSize;
    uint32_t unCompressSize;
    uint16_t fileNameLength;
    uint16_t extenedRecordLength;
}FILE_ZIP_INFO_t;
#pragma pack()


static void zipUpgradeInfoRecord(unsigned char *fileName, uint32_t crc32, unsigned int offset, unsigned int size, UPGRADE_FILE_t *file_record)
{
	ZIP_LOG_INFO("%s offset %d, size %d\n", fileName, offset, size);

    //Toast::instance().show(Toast::INFO, QString(QByteArray((char *)fileName)));
    if(strstr((char *)fileName, ".bin"))
    {
        file_record->binOffset = offset;
        file_record->binSize = size;
        file_record->binCrc32 = crc32;
    }
    else if(strstr((char *)fileName, ".dat"))
    {
        file_record->datOffset = offset;
        file_record->datSize = size;
    }
}

bool zipContextRecord(uint32_t *offset, FILE *file, UPGRADE_FILE_t *file_record)
{
    uint32_t crc32 = 0;
    uint32_t file_size;
    FILE_ZIP_INFO_t info;
    // 读取压缩信息
    memset(&info, 0, sizeof(FILE_ZIP_INFO_t));
	fread((char *)&info, 1, sizeof(FILE_ZIP_INFO_t), file);

    if(info.compressSize != info.unCompressSize ||
        info.fileNameLength > 64 ||
        info.headFlag != 0x4034b50)
    {
        //文件异常
        return false;
    }

    *offset += sizeof(FILE_ZIP_INFO_t);

    char fileName[64] = { 0 };
    if(info.fileNameLength)
    {
		fread(fileName, 1, info.fileNameLength, file);
        *offset += info.fileNameLength;
    }
    if(info.extenedRecordLength)
    {
		fread(fileName, 1, info.extenedRecordLength, file);
        *offset += info.extenedRecordLength;
    }

    zipUpgradeInfoRecord((uint8_t *)fileName, info.crc32, *offset, info.unCompressSize, file_record);

    crc32 = 0;
    char buf[4096];
    file_size = info.compressSize;
    while (file_size > sizeof(buf))
    {
		fread(buf, 1, sizeof(buf), file);

        crc32 = crc32_compute((uint8_t *)buf, sizeof(buf), &crc32);
        *offset += sizeof(buf);
        file_size -= sizeof(buf);
    }
	fread(buf, 1, file_size, file);
    crc32 = crc32_compute((uint8_t *)buf, file_size, &crc32);
    *offset += file_size;
    file_size -= file_size;
	
	ZIP_LOG_INFO("crc %u %u\n", crc32, info.crc32);
    if(crc32 != info.crc32)
    {
        //file error
        return false;
    }
    return true;
}

//升级包的zip文件是不压缩仅打包的, 直接解析格式出文件地址和大小
void easyZipParse(char *filePath, UPGRADE_FILE_t *file_record)
{
    ZIP_LOG_INFO("*****************file parse start***********************\n");
    file_record->fileName = filePath;

	
	FILE *file = fopen(filePath, "rb");

    if(file < 0) {
        ZIP_LOG_INFO("file open failed\n");
        return;
    }

    uint32_t offset = 0;

    uint32_t file_num = 0;
    while(1)
    {
        if(zipContextRecord(&offset,file, file_record)) {
            file_num++;
        } else {
            break;
        }
    }

    fclose(file);

	ZIP_LOG_INFO("file number %d\n",file_num);
	ZIP_LOG_INFO("%s\n", file_record->fileName);
	ZIP_LOG_INFO("%d\n", file_record->binOffset);
	ZIP_LOG_INFO("%d\n", file_record->binSize);
	ZIP_LOG_INFO("%u\n", file_record->binCrc32);
	ZIP_LOG_INFO("%d\n", file_record->datOffset);
	ZIP_LOG_INFO("%d\n", file_record->datSize);
	
    ZIP_LOG_INFO("*****************file parse end***********************\n");
}



