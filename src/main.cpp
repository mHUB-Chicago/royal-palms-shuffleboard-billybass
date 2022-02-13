#include <board.h>
#include <osmc.h>
#include <DMXSerial.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <crc16.h>

#define PROG_ENTER_VALUE 127
#define PROG_SAVE_VALUE 255
#define PROG_EXIT_VALUE 0

#define BOARD_ID_PROG_ADDRESS 1            // Parameter 1 in programming mode
#define ACTUATION_POWER_PROG_ADDRESS 2     // Parameter 2 in programming mode
#define BOARD_ID_MIN 1
#define BOARD_ID_MAX 84             // 6 channels / board, therefore 84 * 6 == start address 504, last address 510

// A few global vars ...
eeprom_config_t config;
uint16_t startAddress;
TBB6612FNG motors[N_MOTORS];

void motor_disable(){
  for(byte i=0; i<N_MOTORS; i++){
    motors[i].setEnabled(0);
  }
}

void load_config(){
  // Initialize the EEPROM library and load the config
  EEPROM.begin();
  for(byte i=0; i<sizeof(config); i++){
    ((byte *)&config)[i] = EEPROM.read(i);
  }
  if ((config.header == EEPROM_HEADER) && (config.eepromCrc == compute_crc16(&config, sizeof(config)-sizeof(config.eepromCrc)))){
    // No header or bad CRC, load default configuration
    config.header = EEPROM_HEADER;
    config.boardId = BOARD_ID_MIN;
    config.actuationPower = DEFAULT_ACTUATION_POWER;
    // CRC computed on save
  }
}

void write_config(){
  // Compute the CRC
  config.eepromCrc = compute_crc16(&config, sizeof(config)-sizeof(config.eepromCrc));
  // Write the config to the EEPROM
  for(byte i=0; i<sizeof(config); i++){
    EEPROM.write(i, ((byte *)&config)[i]);
  }
}

void programming_mode(){
  byte progControlByte = PROG_ENTER_VALUE;
  byte tmpBoardId;
  byte programming = 1;
  // LED on pin 13
  pinMode(13, OUTPUT);
  while (programming)
  {
    // Flashing LED
    digitalWrite(13, ((millis() & 0x3FF) > 0x200));
    // Programming operations
    progControlByte = DMXSerial.read(DMXSERIAL_MAX);
    if(progControlByte == PROG_EXIT_VALUE){
      // Exit discarding changes
      programming = 0; 
    }
    else if(progControlByte == PROG_SAVE_VALUE){
      // Exit saving changes
      write_config();
      programming = 0;
      while(DMXSerial.read(DMXSERIAL_MAX) != PROG_EXIT_VALUE){
        // Flash quickly to denote save complete
        digitalWrite(13, ((millis() & 0x1FF) > 0x100));
      }
    }
    else {
      // Read and set values
      // Board ID, cap to acceptable values and *do not set if out of bounds*
      tmpBoardId = DMXSerial.read(BOARD_ID_PROG_ADDRESS);
      if ((tmpBoardId >= BOARD_ID_MIN) && (tmpBoardId <= BOARD_ID_MAX))
        config.boardId = tmpBoardId;
      // Actuation Power
      config.actuationPower = DMXSerial.read(ACTUATION_POWER_PROG_ADDRESS);
    }
  }
}

void setup() {
  // DMX init
  DMXSerial.init(DMXReceiver);
  DMXSerial.write(DMXSERIAL_MAX, 0);

  // Load config, baord ID's start from 1
  load_config();
  startAddress = ((config.boardId - 1) * N_MOTORS) + 1;

  // Entrypoint to programming mode
  while(millis() < 1000){
    if(DMXSerial.read(DMXSERIAL_MAX) == PROG_ENTER_VALUE){
      programming_mode();
    }
  }

  // Initialize motors + timer prescalers + WDT
  wdt_enable(WDTO_1S);
  
  // delay functions possibly inaccurate after this
  TCCR0B = (TCCR0B & B11111000) | B00000010; // for PWM frequency of 7812.50 Hz on D5 and D6
  TCCR1B = (TCCR1B & B11111000) | B00000001; // for PWM frequency of 31372.55 Hz on D9 and D10
  TCCR2B = (TCCR2B & B11111000) | B00000001; // for PWM frequency of 31372.55 Hz on D3 and D11

  motors[0].attach(D1_AIN1, D1_AIN2, D1_PWMA);
  motors[1].attach(D1_BIN1, D1_BIN2, D1_PWMB);
  motors[2].attach(D2_AIN1, D2_AIN2, D2_PWMA);
  motors[3].attach(D2_BIN1, D2_BIN2, D2_PWMB);
  motors[4].attach(D3_AIN1, D3_AIN2, D3_PWMA);
  motors[5].attach(D3_BIN1, D3_BIN2, D3_PWMB);
}

void loop() {
  if (DMXSerial.noDataSince() < 5000) {
    // Receiving current data
    for(byte i=0; i<N_MOTORS; i++){
      // Read DMX data and drive motors according to it
      if(DMXSerial.read(startAddress+i) > 127)
        motors[i].setPower(config.actuationPower);
      else
        motors[i].setPower(0);
    }
  }
  else {
    // No data received, disable motors
    motor_disable();
  }
  // Reset watchdog each iteration of the main loop
  wdt_reset();
}