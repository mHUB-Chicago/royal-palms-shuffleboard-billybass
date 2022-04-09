#include <osmc.h>
#include <ArduinoRenard.h>
#include <configmanager.h>
#include <board.h>
#include <avr/wdt.h>

// Programming mode opcodes
#define CHANNEL_OFFSET_PROG_ADDRESS 0x0         // Parameter 0 in programming mode
#define UNIDIR_ACTUATION_POWER_PROG_ADDRESS 0x1 // Parameter 1 in programming mode
#define BIDIR_ACTUATION_POWER_PROG_ADDRESS 0x2  // Parameter 2 in programming mode
#define REVERSE_MASK_PROG_ADDRESS 0x3           // Parameter 3 in programming mode
#define MOTOR_ORDER_PROG_ADDRESS 0x4            // Parameter 4 in programming mode
#define RAMP_RATE_PROG_ADDRESS 0x5              // Parameter 5 in programming mode
#define ACTUATION_TEST_PROG_ADDRESS 0xF0        // Parameter 240 in programming mode
#define PROG_EXIT_OPCODE 0xFE
#define PROG_OPCODE 0xFF

//#define VERBOSE_OUTPUT
#define VERSION_STR F("0.5.0")

// A few global vars ...
#define CHANNEL_OFFSET_MAX (RENARD_MAX_ADDRESS - N_CHANNELS)
typedef enum
{
  OPST_RESET,
  OPST_NORMAL,
  OPST_M_DISABLE
} operating_mode_t;

// Globals
operating_mode_t operatingMode = OPST_RESET;
eeprom_config_t config;
TBB6612FNG motors[N_MOTORS];
RenardReceiver dataBus;
unsigned long lastRamp;
const unsigned long rampInterval = 8;

// Timer 0 prescaler config
#if defined(TCCR0B)
const unsigned long timeScale = 8;
void enable_fast_timer0(bool enable)
{
  if (enable)
  {
    // ~8KHz PWM via timer0 configuration (makes millis() tick 8x faster!)
    TCCR0B = (TCCR0B & B11111000) | 0b10;
    dataBus.setIdleTimeout(RENARD_DEFAULT_IDLE_TIMEOUT * 8);
  }
  else
  {
    // Set default timer0 prescaler
    TCCR0B = (TCCR0B & B11111000) | 0b11;
    dataBus.setIdleTimeout(RENARD_DEFAULT_IDLE_TIMEOUT);
  }
}
#else
const unsigned long timeScale = 1;
void enable_fast_timer0(bool enable) { return; };
#endif

void attach_mapped_motor(byte physMotorNum, byte pin1, byte pin2, byte pwm)
{
  /* Attaches and enables a motor,
     remapping the physical ID and applying direction reverse if needed */
  bool reverse = (config.reverseMask >> physMotorNum) & 0x01;
  byte virtMotorNum = map_motor_idx(&config, physMotorNum);
  if (reverse)
    motors[virtMotorNum].attach(pin2, pin1, pwm);
  else
    motors[virtMotorNum].attach(pin1, pin2, pwm);
  motors[virtMotorNum].setEnabled(true);
  motors[virtMotorNum].setRampEnabled(true);
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
    // Fast timer0
    enable_fast_timer0(true);
    // Attach and enable motor 0
    attach_mapped_motor(0, D1_AIN1, D1_AIN2, D1_PWMA);
    break;

  case OPST_M_DISABLE:
    // Stop all motor outputs
    for (byte i = 0; i < N_MOTORS; i++)
      motors[i].setPower(0, false);
    // Detach physical motor 0
    motors[map_motor_idx(&config, 0)].detach();
    // Slow timer0
    enable_fast_timer0(false);
    // Set LED pin as output low
    digitalWrite(LED_BUILTIN, 0);
    pinMode(LED_BUILTIN, OUTPUT);
    break;

  case OPST_RESET:
    // All motors detached
    for (byte i = 0; i < N_MOTORS; i++)
    {
      motors[i].detach();
    }
    // Slow timer0
    enable_fast_timer0(false);
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

void print_startup()
{
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

void actuation_test(byte decisecs)
{
  unsigned long realtime = 100UL * constrain(decisecs, 1, 50);
  unsigned long runtime = timeScale * realtime;
  unsigned long startTime;
  Serial.print(F("Initiating actuation test with on time="));
  Serial.print(realtime);
  Serial.println(F("/10 sec..."));
  for (byte i = 0; i < N_MOTORS; i++)
  {
    Serial.print(F("Motor: "));
    Serial.println(i);
    if (config.actuationPowerBiDir > 0 &&
        (i & 0x1))
    {
      // Bidirectional motor
      motors[i].setPower(config.actuationPowerBiDir, false);
    }
    else
    {
      // Unidirectional motor
      motors[i].setPower(config.actuationPowerUniDir, false);
    }
    // Perform acceleration and reset the watchdog for duration of the test movement
    startTime = millis();
    lastRamp = 0;
    while ((millis() - startTime) < runtime)
    {
      if ((millis() - lastRamp) >= (rampInterval * timeScale))
      {
        motors[i].doRamp(config.rampRate);
        lastRamp = millis();
      }
      wdt_reset();
    }
    motors[i].setPower(0, false);
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
      tmpByte = dataBus.readSpecialOpcode();
      switch (tmpByte)
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
        if (tmpByte < MOTOR_ORDER_COUNT) // bounds check
          config.motorOrder = tmpByte;
        // Reattach all motors after changing this
        set_global_oper_mode(OPST_RESET);
        break;
      case RAMP_RATE_PROG_ADDRESS:
        // Ramp rate, full range
        config.rampRate = dataBus.readSpecialData();
        break;
      case ACTUATION_TEST_PROG_ADDRESS:
        // Tests the actuation of all motors
        set_global_oper_mode(OPST_NORMAL);
        actuation_test(dataBus.readSpecialData());
        break;
      case PROG_EXIT_OPCODE:
        // Save if we see the PROG_EXIT_OPCODE as the data as well
        if (dataBus.readSpecialData() == PROG_EXIT_OPCODE)
        {
          digitalWrite(LED_BUILTIN, 1);
          write_config(&config);
          delay(1000);
          digitalWrite(LED_BUILTIN, 0);
        }
        // Then exit
        programming = 0;
        break;
      case PROG_OPCODE:
        dataBus.readSpecialData(); // Discard the parameter
        break;                     // Prints the current config
      default:
        dataBus.readSpecialData(); // Discard the parameter
        Serial.print(F("Invalid programming opcode: 0x"));
        Serial.println(tmpByte, HEX);
        continue; // Don't print the config
      }
      print_config(&config);
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

  // Load config, blink 3x on error
  EEPROM.begin();
  if (!load_config(&config))
    blink_code(3, 1500);
  print_config(&config);

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

// Enable ~31KHz PWM on timers 1 and 2 if supported
// Timer 0 only set to nonstandard frequency in operation as this makes millis() inaccurate!
#if defined(TCCR1B) && defined(TCCR2B) && defined(CS00)
  TCCR1B = (TCCR1B & 0b11111000) | _BV(CS00);
  TCCR2B = (TCCR2B & 0b11111000) | _BV(CS00);
#endif

  Serial.println(F("---Ready to operate---"));
  lastRamp = millis();
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
  bool needRamp;
  if (!dataBus.isIdle())
  {
    // Receiving valid data
    set_global_oper_mode(OPST_NORMAL);
    needRamp = ((millis() - lastRamp) >= (rampInterval * timeScale));
    for (byte i = 0; i < N_MOTORS; i++)
    {
      tmpA = dataBus.read(channel++);
      if (config.actuationPowerBiDir > 0 &&
          (i & 0x1))
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
      // Evaluate for accel. ramping
      if (needRamp)
        motors[i].doRamp(config.rampRate);
    }
    // Reset accel timer if we just ramped up power
    if (needRamp)
      lastRamp = millis();
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