#ifndef CRC64_H
#define CRC64_H

#include <stdint.h>

/* CRC needs to be inited (eg to 0). There is no XOR-out operation so this
 * function may be called multiple times to generate a CRC on large data */
uint64_t crc64(uint64_t crc, const unsigned char *s, unsigned int len);

#endif
