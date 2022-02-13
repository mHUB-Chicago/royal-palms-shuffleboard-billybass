#include <crc16.h>
/* http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
 */
uint16_t compute_crc16(void *data, unsigned int len){
        uint16_t crc = 0xffff;
        int i;
        for(i=0; i<len; i++){
                crc=_crc16_update(crc, ((uint8_t*)data)[i]);
        }
        return crc;
}