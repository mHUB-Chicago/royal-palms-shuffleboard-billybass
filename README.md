# Royal Palms Billy Bass Controller - A platform to control animatronic singing fish at scale

This repository contains the software designed to operate the installation of the 70+ Big Mouth Billy Bass
located at the [Royal Palms Shuffleboard Club](https://www.royalpalmschicago.com/) located in Chicago, IL.

However, this code could easily be adapted to control large numbers of pretty much any other type of 
motor, solenoid, or servo operated animatronic prop using readily available show sequencing tools such as [xLights](https://xlights.org/)

## APIs
### Operating command API
This software implements the [Renard serial protocol](https://www.doityourselfchristmas.com/wiki/index.php?title=Renard#Protocol) for normal operation since it can operate readily over USB-serial,
as well as RS485 in order to ensure compatiblity with a wide variety of sequencing and show design tools.

Each board occupies 3 groups of 3 channels, one group per motor driver, for a total of 9 total channels. The channels *in each group* are assigned to motions as follows:
1. Controls the unidirectional motor A output. This is typically the mouth of the Billy Bass.
2. Controls the bidirectional motor B output in the + direction. This typically moves the head out from the backboard.
3. Controls the bidirectional motor B output in the - direction. This typically lifts the tail up from the backboard.

Channels are considered asserted when assigned a value greater than 127 units and the assertion of channel 2 will take priority over channel 3.

### Programming / Configuration API
This software supports in-situ configuration of several parameters governing its operation in order to simplify installation as well as reconfiguration. These parameters are saved to EEPROM and persist across power loss and can be configured via programming/special commands of the form:

    0x7e (start of frame)
    0x7a (programming/special command)
    0xNN (programming opcode/parameter, NN is equal to the parameter number as stated at the top of main.cpp)
    0xNN (programming data, NN is equal to the data / argument. This is always required, but not interpreted for all commands.)

#### Entering programming mode
In order to set configuration parameters, programming mode must first be entered by sending a programming command with opcode 0xFF addressed to the intended board. This can be accomplished via one of 2 ways:
1. Upon powerup, there is a 3 second window during which the indicator LED will flash at 8Hz. During this time, send a programming command with opcode 0xFF with *any argument*, for example 0x7E, 0x7A, 0xFF, 0xFF.
2. During normal operation, send a programming command with opcode 0xFF and argument value *equal to the target board's channel offset*. For example to put board with channel offset 0 (the default) into programming mode, send 0x7E, 0x7A, 0xFF, 0x00.

Successful entry into programming mode can be confirmed via a message printed via the serial output as well as the LED blinking at 2Hz.

#### Programmable parameters
*(default values defined in board.h)*
* CHANNEL_OFFSET(0x0): Defines the numerical offset from channel 1 that the first channel on this board will respond to.
* UNIDIR_ACTUATION_POWER(0x1): Sets the amount of motor power (0-255) applied to actuate the unidirectional motors (typically the mouth).
* BIDIR_ACTUATION_POWER(0x2): Sets the amount of motor power (0-255) applied to actuate the bidirectional motors (typically the head/tail).
* REVERSE_MASK(0x3): Bitmask defining which motors (in terms of *physical output number*) shall have their actuation direction reversed.
* MOTOR_ORDER(0x4): Defines the index into the *motor_reorder_tbl* (defined in board.h) used to swap/remap the order of physical output pairs.
* ACTUATION_TEST(0xF0): When invoked, each motor is actuated in the + direction for 0.1 * (argument) seconds in order of increasing logical channel number. *actuation time is capped to 5s*

#### Exiting programming mode
To exit programming mode, send a programming command with opcode **0xFE** and data as follows:
* To save changes to the EEPROM, set the data field equal to **0xFE**.
* To exit without saving changes (such that they will be lost upon power cycle), set the data field to any other value.