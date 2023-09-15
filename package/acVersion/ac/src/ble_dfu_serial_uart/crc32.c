#include "crc32.h"
#include <stdio.h>

/**
 * @brief nbDfu::crc32_compute
 * @param p_data 数据源
 * @param size 数据大小
 * @param p_crc NULL: 仅计算本次  非NULL:配合上次计算的值计算下一段数据,用来分段计算大量数据使用
 * @return
 */
uint32_t crc32_compute(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc)
{
    uint32_t crc;

    crc = (p_crc == NULL) ? 0xFFFFFFFF : ~(*p_crc);
    for (uint32_t i = 0; i < size; i++)
    {
        crc = crc ^ p_data[i];
        for (uint32_t j = 8; j > 0; j--)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & ((crc & 1) ? 0xFFFFFFFF : 0));
        }
    }
    return ~crc;
}

