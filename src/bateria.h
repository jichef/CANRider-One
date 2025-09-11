#pragma once
#include <Arduino.h>

extern uint8_t soc;  // SoC crudo (último leído de CAN)

// Llama a esto al recibir CAN 0x541 (data[0] = SoC)
void battery_onCan541(const uint8_t* data, uint8_t len);

// Lecturas robustas
uint8_t soc_effective();
bool    soc_critico_confirmado();

// Lógica existente
void checkSoC();
void checkSoCOnBoot();
