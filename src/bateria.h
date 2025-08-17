#ifndef BATERIA_H
#define BATERIA_H


#include <Arduino.h>

void checkSoC();
void checkSoCBoot();
extern uint8_t soc;
extern bool apagado_ya;

#endif
