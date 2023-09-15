#include "crc32.h"
#include <stdio.h>

/**
 * @brief nbDfu::crc32_compute
 * @param p_data ����Դ
 * @param size ���ݴ�С
 * @param p_crc NULL: �����㱾��  ��NULL:����ϴμ����ֵ������һ������,�����ֶμ����������ʹ��
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

