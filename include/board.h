#include <Arduino.h>

#define EEPROM_HEADER 0xCAFE
typedef struct __attribute__((packed)){
    uint16_t header;
    byte boardId;
    byte actuationPower;
    uint16_t eepromCrc;
} eeprom_config_t;


#define N_MOTORS 6
#define DEFAULT_ACTUATION_POWER 192

#define D1_AIN1 12
#define D1_AIN2 13
#define D1_PWMA 11
#define D1_BIN1 8
#define D1_BIN2 7
#define D1_PWMB 10
#define D2_AIN1 2
#define D2_AIN2 4
#define D2_PWMA 6
#define D2_BIN1 A2
#define D2_BIN2 A3
#define D2_PWMB 9
#define D3_AIN1 A4
#define D3_AIN2 A5
#define D3_PWMA 3
#define D3_BIN1 A1
#define D3_BIN2 A0
#define D3_PWMB 5