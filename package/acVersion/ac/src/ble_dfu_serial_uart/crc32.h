#ifndef CRC32_H__
#define CRC32_H__

#include <stdint.h>

uint32_t crc32_compute(uint8_t const * p_data, uint32_t size, uint32_t const * p_crc);


#endif //CRC32_H__
