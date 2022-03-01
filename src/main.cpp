#include <board.h>
#include <osmc.h>
#include <renard.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <crc16.h>

#define PROG_OPCODE 0xFF
#define PROG_EXIT_OPCODE 0xFE

#define N_CHANNELS 9
#define BOARD_ID_PROG_ADDRESS 0        // Parameter 0 in programming mode
#define ACTUATION_POWER_PROG_ADDRESS 1 // Parameter 1 in programming mode
#define BOARD_ID_DEFAULT 0
#define BOARD_ID_MAX 27 // 9 channels / board, therefore 27 * 9 == last board start address 243, last address 252

#define VERSION_STR F("0.1.0")

// A few global vars ...
eeprom_config_t config;
TBB6612FNG motors[N_MOTORS];
RenardReceiver dataBus;

void blink_code(byte n, unsigned int onTime)
{
  for (byte i = 0; i < n; i++)
  {
    digitalWrite(LED_BUILTIN, 1);
    delay(onTime);
    digitalWrite(LED_BUILTIN, 0);
    delay(500);
  }
}

void motor_disable()
{
  for (byte i = 0; i < N_MOTORS; i++)
  {
    motors[i].setEnabled(0);
  }
}

void load_config()
{
  // Initialize the EEPROM library and load the config
  Serial.println(F("Loading board configuration..."));
  EEPROM.begin();
  for (byte i = 0; i < sizeof(config); i++)
  {
    ((byte *)&config)[i] = EEPROM.read(i);
  }
  if ((config.header != EEPROM_HEADER) || (config.eepromCrc != compute_crc16(&config, sizeof(config) - sizeof(config.eepromCrc))))
  {
    // No header or bad CRC, load default configuration
    config.header = EEPROM_HEADER;
    config.boardId = BOARD_ID_DEFAULT;
    config.actuationPower = DEFAULT_ACTUATION_POWER;
    Serial.println(F("EEPROM header or CRC is invalid! Using defaults."));
    blink_code(3, 1500);
    // CRC computed on save
  }
  Serial.print(F("Config: board ID="));
  Serial.print(config.boardId);
  Serial.print(F(", actuation power="));
  Serial.println(config.actuationPower);
}

void write_config()
{
  Serial.println(F("Writing board configuration to EEPROM..."));
  // Compute the CRC
  config.eepromCrc = compute_crc16(&config, sizeof(config) - sizeof(config.eepromCrc));
  // Write the config to the EEPROM
  for (byte i = 0; i < sizeof(config); i++)
  {
    EEPROM.write(i, ((byte *)&config)[i]);
  }
  Serial.println(F("Complete!"));
}

void programming_mode()
{
  Serial.println(F("Programming mode entered"));
  byte programming = 1, tmpBoardId;
  while (programming)
  {
    // Flashing LED
    digitalWrite(LED_BUILTIN, ((millis() & 0x1FF) > 0xFF));
    if (dataBus.specialAvailable())
    {
      // Programming operations, always need to read the special data
      switch (dataBus.readSpecialOpcode())
      {
      case BOARD_ID_PROG_ADDRESS:
        // Board ID, cap to acceptable values and *do not set if out of bounds*
        tmpBoardId = dataBus.readSpecialData();
        if (tmpBoardId <= BOARD_ID_MAX){
          Serial.print(F("Board ID set to "));
          Serial.println(tmpBoardId);
          config.boardId = tmpBoardId;
        }
        break;
      case ACTUATION_POWER_PROG_ADDRESS:
        // Actuation power, full range
        config.actuationPower = dataBus.readSpecialData();
        Serial.print(F("Actuation power set to "));
        Serial.println(config.actuationPower);
        break;
      case PROG_EXIT_OPCODE:
        // Save if we see the PROG_EXIT_OPCODE as the data as well
        if (dataBus.readSpecialData() == PROG_EXIT_OPCODE)
        {
          digitalWrite(LED_BUILTIN, 1);
          write_config();
          delay(1000);
          digitalWrite(LED_BUILTIN, 0);
        }
        // Then exit
        programming = 0;
        break;
      }
    }
  }
}

void setup()
{
  // LED on pin 13 used for status reporting on startup
  pinMode(LED_BUILTIN, OUTPUT);

  // Renard (+ Serial) init
  dataBus.begin(N_CHANNELS, &Serial, 115200);
  Serial.print(F("Royal Palms Billy Bass Controller v"));
  Serial.println(VERSION_STR);

  // Load config
  load_config();

  // Entrypoint to programming mode
  Serial.println(F("Checking for command to enter programming mode..."));
  unsigned long progTestTime = millis();
  while ((millis() - progTestTime) < 3000)
  {
    // Flash quickly to denote ready to program
    digitalWrite(LED_BUILTIN, ((millis() & 0x80) > 0x40));
    if (dataBus.specialAvailable() && dataBus.readSpecialOpcode() == PROG_OPCODE)
    {
      dataBus.readSpecialData(); // Discard the data and reset the special command pending flag
      programming_mode();
    }
  }

  // Apply channel offset derived from board ID
  dataBus.setChannelOffset(config.boardId * N_CHANNELS);

  // Initialize motors + timer prescalers + WDT
  // LED on pin 13 used for motor control now
  digitalWrite(LED_BUILTIN, 0);
  Serial.println(F("---Ready to operate---"));
  pinMode(LED_BUILTIN, INPUT);
  wdt_enable(WDTO_1S);

  // delay functions possibly inaccurate after this
  // TCCR0B = (TCCR0B & B11111000) | B00000010; // for PWM frequency of 7812.50 Hz on D5 and D6
  // TCCR1B = (TCCR1B & B11111000) | B00000001; // for PWM frequency of 31372.55 Hz on D9 and D10
  // TCCR2B = (TCCR2B & B11111000) | B00000001; // for PWM frequency of 31372.55 Hz on D3 and D11

  // motor 0 attached later as pins are shared with status LED
  motors[1].attach(D1_BIN1, D1_BIN2, D1_PWMB);
  motors[2].attach(D2_AIN1, D2_AIN2, D2_PWMA);
  motors[3].attach(D2_BIN1, D2_BIN2, D2_PWMB);
  motors[4].attach(D3_AIN1, D3_AIN2, D3_PWMA);
  motors[5].attach(D3_BIN1, D3_BIN2, D3_PWMB);
}

inline byte threshold(byte val){
  if (val > 127)
    return config.actuationPower;
  else
    return 0;
}

void loop()
{
  byte channel=0, tmpA, tmpB, bidirPower;
  if (!dataBus.isIdle())
  {
    // Re-attach motor 0 if needed
    if(!motors[0].attached())
      motors[0].attach(D1_AIN1, D1_AIN2, D1_PWMA);
    // Receiving current data
    for (byte i = 0; i < N_MOTORS;i++)
    {
      tmpA = dataBus.read(channel++);
      motors[i].setEnabled(1);
      if(i & 0x1){
        // Bidirectional motor
        tmpB = dataBus.read(channel++, false);
        bidirPower = threshold(max(tmpA, tmpB));
        motors[i].setPower(bidirPower, (tmpB >= tmpA));
      }
      else{
        // Unidirectional motor
        motors[i].setPower(threshold(tmpA), false);
      }
    }
  }
  else
  {
    // No data received, disable motors
    motor_disable();
    // then Detach motor 0 and blink the LED at 1hz
    motors[0].detach();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, ((millis() & 0x3FF) > 0x200));
    // As well as watch for incoming data
    dataBus.process();
  }
  // Reset watchdog each iteration of the main loop
  wdt_reset();
}