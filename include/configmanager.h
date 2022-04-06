#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <crc16.h>

// EEPROM configuration struct
#define EEPROM_HEADER 0xCAFF
#define EEPROM_LEGACY_HEADER 0xCAFE

#define EEPROM_HEADER_SIZE sizeof(((eeprom_config_t *)0)->header)

typedef struct __attribute__((packed))
{
    uint16_t header;
    byte channelOffset;
    byte actuationPowerUniDir;
    byte actuationPowerBiDir;
    byte reverseMask;
    byte motorOrder;
    uint16_t eepromCrc;
} eeprom_legacy_config_t;

typedef struct __attribute__((packed))
{
    uint16_t header;
    byte channelOffset;
    byte actuationPowerUniDir;
    byte actuationPowerBiDir;
    byte reverseMask;
    byte motorOrder;
    byte rampRate;
    uint16_t eepromCrc;
} eeprom_config_t;

// Default vaules when the above struct in EEPROM is invalid
#define DEFAULT_CHANNEL_OFFSET 0
#define DEFAULT_UNIDIR_ACTUATION_POWER 192
#define DEFAULT_BIDIR_ACTUATION_POWER 255
#define DEFAULT_REVERSE_MASK 0x0
#define DEFAULT_MOTOR_ORDER 0x0
#define DEFAULT_RAMP_RATE 16

void print_config(eeprom_config_t *cfg);
bool load_config(eeprom_config_t *cfg);
void write_config(eeprom_config_t *cfg);
void print_padded_binary(byte in, HardwareSerial *port);
byte map_motor_idx(eeprom_config_t *cfg, byte inIdx);