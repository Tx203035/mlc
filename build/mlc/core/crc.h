#ifndef __MLC_CRC_H__
#define __MLC_CRC_H__

#include "core.h"

/* 32-bit crc16 */

static inline uint32_t
mlc_crc(const void *data, size_t len)
{
    uint32_t sum;
    const uint8_t *p = (const uint8_t *)data;

    for (sum = 0; len; len--)
    {

        /*
         * gcc 2.95.2 x86 and icc 7.1.006 compile
         * that operator into the single "rol" opcode,
         * msvc 6.0sp2 compiles it into four opcodes.
         */
        sum = sum >> 1 | sum << 31;

        sum += *p++;
    }

    return sum;
}

#endif