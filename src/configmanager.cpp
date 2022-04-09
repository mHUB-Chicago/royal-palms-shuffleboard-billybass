#include <configmanager.h>
#include <board.h>

void print_padded_binary(byte in, HardwareSerial *port)
{
    char ascii[] = {'0', '1'};
    byte i = 8;
    port->print("0b");
    while (i > 0)
    {
        port->print(ascii[(in >> --i) & 1]);
    }
}

void print_config(eeprom_config_t *cfg)
{
    Serial.print(F("Config: channel offset=0x"));
    Serial.print(cfg->channelOffset, HEX);
    Serial.print("(");
    Serial.print(F("start channel "));
    Serial.print(1 + cfg->channelOffset, DEC);
    Serial.print(")");
    Serial.print(F(", unidirectional actuation power="));
    Serial.print(cfg->actuationPowerUniDir);
    Serial.print(F(", bidirectional actuation power="));
    Serial.print(cfg->actuationPowerBiDir);
    Serial.print(F(", ramp rate(units/8ms)="));
    Serial.print(cfg->rampRate);
    Serial.print(F(", reverseMask=0x"));
    Serial.print(cfg->reverseMask, HEX);
    Serial.print("(");
    print_padded_binary(cfg->reverseMask, &Serial);
    Serial.println(F(")"));
    Serial.print(F("\tMotor mapping (virt -> phy): "));
    for (byte i = 0; i < N_MOTORS; i++)
    {
        Serial.print(i);
        Serial.print(F("->"));
        Serial.print(map_motor_idx(cfg, i));
        if (i != (N_MOTORS - 1))
            Serial.print(F(", "));
    }
    Serial.println();
}

bool load_config(eeprom_config_t *cfg)
{
    bool configOk = false;
    eeprom_legacy_config_t legacyCfg;

    // Initialize the EEPROM library and load the header
    Serial.println(F("Loading board configuration..."));
    eeprom_read_block(cfg, 0, EEPROM_HEADER_SIZE);
    switch (cfg->header)
    {
    case EEPROM_HEADER:
        // Current version's config header, therefore try to read as such
        eeprom_read_block(cfg, 0, sizeof(eeprom_config_t));
        if (cfg->eepromCrc == compute_crc16((byte *)cfg, sizeof(eeprom_config_t) - sizeof(cfg->eepromCrc)))
        {
            Serial.println(F("Loaded up-to-date configuration from EEPROM."));
            configOk = true;
        }
        break;
    case EEPROM_LEGACY_HEADER:
        // Version N-1's config header, therefore try to read and update
        eeprom_read_block(&legacyCfg, 0, sizeof(eeprom_legacy_config_t));
        if (legacyCfg.eepromCrc == compute_crc16((byte *)&legacyCfg, sizeof(eeprom_legacy_config_t) - sizeof(legacyCfg.eepromCrc)))
        {
            configOk = true;
            Serial.println(F("Loaded legacy configuration, applying defaults for added parameters."));
            /* begin config upgrade handling */
            memcpy(cfg, &legacyCfg, sizeof(eeprom_legacy_config_t));
            cfg->header = EEPROM_HEADER;
            cfg->rampRate = DEFAULT_RAMP_RATE;
            cfg->actuationPowerBiDir = DEFAULT_BIDIR_ACTUATION_POWER;
            /* end config upgrade handling */
            write_config(cfg);
        }
        break;
    }

    if (!configOk)
    {
        // No header or bad CRC, load default configuration
        cfg->header = EEPROM_HEADER;
        cfg->channelOffset = DEFAULT_CHANNEL_OFFSET;
        cfg->actuationPowerUniDir = DEFAULT_UNIDIR_ACTUATION_POWER;
        cfg->actuationPowerBiDir = DEFAULT_BIDIR_ACTUATION_POWER;
        cfg->reverseMask = DEFAULT_REVERSE_MASK;
        cfg->motorOrder = DEFAULT_MOTOR_ORDER;
        cfg->rampRate = DEFAULT_RAMP_RATE;
        Serial.println(F("EEPROM header or CRC is invalid! Using defaults."));
    }
    return configOk;
}

void write_config(eeprom_config_t *cfg)
{
    Serial.println(F("Writing board configuration to EEPROM..."));
    // Compute the CRC
    cfg->eepromCrc = compute_crc16((byte *)cfg, sizeof(eeprom_config_t) - sizeof(cfg->eepromCrc));
    // Write the config to the EEPROM
    for (byte i = 0; i < sizeof(eeprom_config_t); i++)
    {
        EEPROM.update(i, ((byte *)cfg)[i]);
    }
    Serial.println(F("Complete!"));
}

byte map_motor_idx(eeprom_config_t *cfg, byte inIdx)
{
    /* Remaps a motor index from the "virtual" motor number to a "physical" motor number,
       by mapping pairs of motors (one pair per output driver)
       per the value of the motorOrder config option */
    byte motorOrder, pairIdx;
    motorOrder = (cfg->motorOrder < MOTOR_ORDER_COUNT) ? cfg->motorOrder : 0;
    pairIdx = inIdx >> 1;
    return ((motor_reorder_tbl[motorOrder][pairIdx]) << 1) | (inIdx & 1);
}