#pragma once
#include <Arduino.h>

// Motor and channel count, motor count must be even
#define N_MOTORS 6
#define N_MOTOR_PAIRS (N_MOTORS >> 1)
#define N_CHANNELS (N_MOTOR_PAIRS * 3)

// How many motor pair reorderings? Max will be factorial(N_MOTORS)
#define MOTOR_ORDER_COUNT 6
const byte motor_reorder_tbl[MOTOR_ORDER_COUNT][N_MOTOR_PAIRS] = {{0, 1, 2},
                                                                  {1, 2, 0},
                                                                  {2, 0, 1},
                                                                  {0, 2, 1},
                                                                  {2, 1, 0},
                                                                  {1, 0, 2}};

// Serial baudrate
#define BAUD 115200

// Hardware pin mappings
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