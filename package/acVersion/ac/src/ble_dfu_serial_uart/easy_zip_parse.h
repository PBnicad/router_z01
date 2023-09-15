#ifndef EASY_ZIP_PARSE_H__
#define EASY_ZIP_PARSE_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *fileName;
    uint32_t binOffset;
    uint32_t binCrc32;
    uint32_t binSize;
    uint32_t datOffset;
    uint32_t datSize;
}UPGRADE_FILE_t;


void easyZipParse(char *filePath, UPGRADE_FILE_t *file_record);


#endif //EASY_ZIP_PARSE_H__

