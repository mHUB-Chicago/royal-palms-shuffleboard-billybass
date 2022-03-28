#include <board.h>
#include <osmc.h>
#include <ArduinoRenard.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <crc16.h>

// Programming mode opcodes
#define CHANNEL_OFFSET_PROG_ADDRESS 0x0         // Parameter 0 in programming mode
#define UNIDIR_ACTUATION_POWER_PROG_ADDRESS 0x1 // Parameter 1 in programming mode
#define BIDIR_ACTUATION_POWER_PROG_ADDRESS 0x2  // Parameter 2 in programming mode
#define REVERSE_MASK_PROG_ADDRESS 0x3           // Parameter 3 in programming mode
#define MOTOR_ORDER_PROG_ADDRESS 0x4            // Parameter 4 in programming mode
#define ACTUATION_TEST_PROG_ADDRESS 0xF0        // Parameter 240 in programming mode
#define PROG_EXIT_OPCODE 0xFE
#define PROG_OPCODE 0xFF

//#define VERBOSE_OUTPUT
#define VERSION_STR F("0.4.0")

// A few global vars ...
#define CHANNEL_OFFSET_MAX (RENARD_MAX_ADDRESS - N_CHANNELS)
typedef enum
{
  OPST_RESET,
  OPST_NORMAL,
  OPST_M_DISABLE
} operating_mode_t;
operating_mode_t operatingMode = OPST_RESET;
eeprom_config_t config;
TBB6612FNG motors[N_MOTORS];
RenardReceiver dataBus;

byte map_motor_idx(byte inIdx)
{
  /* Remaps a motor index from the "virtual" motor number to a "physical" motor number,
     by mapping pairs of motors (one pair per output driver)
     per the value of the motorOrder config option */
  byte motorOrder, pairIdx;
  motorOrder = (config.motorOrder < MOTOR_ORDER_COUNT) ? config.motorOrder : 0;
  pairIdx = inIdx >> 1;
  return ((motor_reorder_tbl[motorOrder][pairIdx]) << 1) | (inIdx & 1);
}

void attach_mapped_motor(byte physMotorNum, byte pin1, byte pin2, byte pwm){
  /* Attaches a motor,
     remapping the physical ID and applying direction reverse if needed */
  bool reverse = (config.reverseMask >> physMotorNum) & 0x01;
  byte virtMotorNum = map_motor_idx(physMotorNum);
  if(reverse)
    motors[virtMotorNum].attach(pin2, pin1, pwm);
  else
    motors[virtMotorNum].attach(pin1, pin2, pwm);
}

void set_global_oper_mode(operating_mode_t nextMode)
{
  /* Sets the operating mode of the platform,
     reconfiguring hardware as needed */
  if (operatingMode == nextMode)
  {
    // No change in state == return immediately
    return;
  }
  else if (operatingMode == OPST_RESET)
  {
    // We are transitioning out of the init/reset state,
    // attach all motors; physical motor 0 attached later as pins are shared with status LED
    attach_mapped_motor(1, D1_BIN1, D1_BIN2, D1_PWMB);
    attach_mapped_motor(2, D2_AIN1, D2_AIN2, D2_PWMA);
    attach_mapped_motor(3, D2_BIN1, D2_BIN2, D2_PWMB);
    attach_mapped_motor(4, D3_AIN1, D3_AIN2, D3_PWMA);
    attach_mapped_motor(5, D3_BIN1, D3_BIN2, D3_PWMB);
  }
  switch (nextMode)
  {
  case OPST_NORMAL:
    // Normal mode: motors attached and enabled
    // LED pin to input low
    pinMode(LED_BUILTIN, INPUT);
    digitalWrite(LED_BUILTIN, 0);
    // Attach motor 0
    attach_mapped_motor(0, D1_AIN1, D1_AIN2, D1_PWMA);
    // All motors enabled and set to stopped
    for (byte i = 0; i < N_MOTORS; i++)
    {
      motors[i].setPower(0, false);
      motors[i].setEnabled(1);
    }
    break;

  case OPST_M_DISABLE:
    // Disable all motor outputs
    for (byte i = 0; i < N_MOTORS; i++)
      motors[i].setEnabled(0);
    // then detach physical motor 0 and set LED pin as output low
    motors[map_motor_idx(0)].detach();
    digitalWrite(LED_BUILTIN, 0);
    pinMode(LED_BUILTIN, OUTPUT);
    break;

  case OPST_RESET:
    // All motors detached
    for (byte i = 0; i < N_MOTORS; i++)
    {
      motors[i].detach();
    }
    break;
  };
  operatingMode = nextMode;
}

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

void print_startup(){
  Serial.print(F("Royal Palms Billy Bass Controller v"));
  Serial.println(VERSION_STR);
  #ifdef VERBOSE_OUTPUT
  Serial.print(F("Hardware: "));
  Serial.print(N_MOTORS);
  Serial.print(F(" motors, "));
  Serial.print(N_CHANNELS);
  Serial.println(F(" channels"));
  #endif
}

void print_config()
{
  Serial.print(F("Config: channel offset=0x"));
  Serial.print(config.channelOffset, HEX);
  Serial.print("(");
  Serial.print(F("start channel "));
  Serial.print(1 + config.channelOffset, DEC);
  Serial.print(")");
  Serial.print(F(", unidirectional actuation power="));
  Serial.print(config.actuationPowerUniDir);
  Serial.print(F(", bidirectional actuation power="));
  Serial.print(config.actuationPowerBiDir);
  Serial.print(F(", reverseMask=0x"));
  Serial.print(config.reverseMask, HEX);
  Serial.print("(");
  print_padded_binary(config.reverseMask, &Serial);
  Serial.println(F(")"));
  Serial.print(F("\tMotor mapping (virt -> phy): "));
  for (byte i = 0; i < N_MOTORS; i++)
  {
    Serial.print(i);
    Serial.print(F("->"));
    Serial.print(map_motor_idx(i));
    if (i != (N_MOTORS - 1))
      Serial.print(F(", "));
  }
  Serial.println();
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
    config.channelOffset = DEFAULT_CHANNEL_OFFSET;
    config.actuationPowerUniDir = DEFAULT_UNIDIR_ACTUATION_POWER;
    config.actuationPowerBiDir = DEFAULT_BIDIR_ACTUATION_POWER;
    config.reverseMask = DEFAULT_REVERSE_MASK;
    config.motorOrder = DEFAULT_MOTOR_ORDER;
    Serial.println(F("EEPROM header or CRC is invalid! Using defaults."));
    blink_code(3, 1500);
    // CRC computed on save
  }
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

void actuation_test()
{
  Serial.println(F("Initiating actuation test..."));
  for (byte i = 0; i < N_MOTORS; i++)
  {
    Serial.print(F("Motor: "));
    Serial.println(i);
    if (i & 0x1)
    {
      // Bidirectional motor
      motors[i].setPower(config.actuationPowerBiDir, false);
    }
    else
    {
      // Unidirectional motor
      motors[i].setPower(config.actuationPowerUniDir, false);
    }
    delay(2000);
    motors[i].setPower(0);
    wdt_reset();
  }
}

void programming_mode()
{
  Serial.println(F("Programming mode entered"));
  byte programming = 1, tmpByte;
  while (programming)
  {
    // Motors off
    set_global_oper_mode(OPST_M_DISABLE);
    // Reset WDT
    wdt_reset();
    // Flashing LED
    digitalWrite(LED_BUILTIN, ((millis() & 0x1FF) > 0xFF));
    if (dataBus.specialAvailable())
    {
      // Programming operations, always need to read the special data
      switch (dataBus.readSpecialOpcode())
      {
      case CHANNEL_OFFSET_PROG_ADDRESS:
        // Channel offset, cap to acceptable values and *do not set if out of bounds*
        tmpByte = dataBus.readSpecialData();
        if (tmpByte <= CHANNEL_OFFSET_MAX)
          config.channelOffset = tmpByte;
        break;
      case UNIDIR_ACTUATION_POWER_PROG_ADDRESS:
        // Actuation power, full range
        config.actuationPowerUniDir = dataBus.readSpecialData();
        break;
      case BIDIR_ACTUATION_POWER_PROG_ADDRESS:
        // Actuation power, full range
        config.actuationPowerBiDir = dataBus.readSpecialData();
        break;
      case REVERSE_MASK_PROG_ADDRESS:
        // Motor reverse bitmask
        config.reverseMask = dataBus.readSpecialData();
        // Reattach all motors after changing this
        set_global_oper_mode(OPST_RESET);
        break;
      case MOTOR_ORDER_PROG_ADDRESS:
        // Motor reorder lookup table entry
        tmpByte = dataBus.readSpecialData();
        if (tmpByte < MOTOR_ORDER_COUNT)  // bounds check
          config.motorOrder = tmpByte;
        // Reattach all motors after changing this
        set_global_oper_mode(OPST_RESET);
        break;
      case ACTUATION_TEST_PROG_ADDRESS:
        // Tests the actuation of all motors
        dataBus.readSpecialData(); // Discard the parameter, it's not used
        set_global_oper_mode(OPST_NORMAL);
        actuation_test();
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
      print_config();
    }
  }
}

void setup()
{
  // LED on pin 13 used for status reporting on startup
  pinMode(LED_BUILTIN, OUTPUT);

  // Renard (+ Serial) init
  dataBus.begin(N_CHANNELS, &Serial, BAUD);

  // Print startup message
  print_startup();

  // Load config
  load_config();
  print_config();

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
  dataBus.setChannelOffset(config.channelOffset);

  Serial.println(F("---Ready to operate---"));
  wdt_enable(WDTO_4S);
}

inline byte threshold(byte val, byte power)
{
  if (val > 127)
    return power;
  else
    return 0;
}

void loop()
{
  byte channel = 0, tmpA, tmpB, bidirPower;
  if (!dataBus.isIdle())
  {
    // Receiving current data
    set_global_oper_mode(OPST_NORMAL);
    for (byte i = 0; i < N_MOTORS; i++)
    {
      tmpA = dataBus.read(channel++);
      if (i & 0x1)
      {
        // Bidirectional motor
        tmpB = dataBus.read(channel++, false);
        bidirPower = threshold(max(tmpA, tmpB), config.actuationPowerBiDir);
        motors[i].setPower(bidirPower, (tmpA < tmpB));
      }
      else
      {
        // Unidirectional motor
        motors[i].setPower(threshold(tmpA, config.actuationPowerUniDir), false);
      }
    }
  }
  else
  {
    // No data received, disable motors and blink no signal code
    set_global_oper_mode(OPST_M_DISABLE);
    digitalWrite(LED_BUILTIN, ((millis() & 0x3FF) > 0x200));
  }

  // Check for a directly addressed programming mode command and always process data
  if (dataBus.specialAvailable() && (dataBus.readSpecialOpcode() == PROG_OPCODE && dataBus.readSpecialData() == config.channelOffset))
  {
    // Enter programming mode and reset the channel offset after we return
    programming_mode();
    dataBus.setChannelOffset(config.channelOffset);
    Serial.println(F("---Ready to operate---"));
  }
  // Reset watchdog each iteration of the main loop
  wdt_reset();
}