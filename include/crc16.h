#pragma once
#ifdef __cplusplus
extern "C"{
#endif

// Use ASM optimised version for arduino
#include <util/crc16.h>
uint16_t compute_crc16(void *data, unsigned int len);

#ifdef __cplusplus
} // extern "C"
#endif