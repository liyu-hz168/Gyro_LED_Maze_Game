#ifndef MAZES_H    
#define MAZES_H

#include <stdint.h>

extern uint8_t maze0[32][32];  
extern uint8_t maze1[32][32];  
extern uint8_t maze2[32][32];
extern uint8_t maze3[32][32];
// extern uint8_t maze4[32][32];
extern uint8_t DEATH[32][32];
extern uint8_t WIN[32][32];

uint8_t (*getMaze(int number))[32];

#endif