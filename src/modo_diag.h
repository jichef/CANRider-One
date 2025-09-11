#ifndef MODO_DIAG_H
#define MODO_DIAG_H

#include <Arduino.h>

void handleModoDiagnostico();
bool modoDiagnosticoActivo();
void activarModoDiagnostico();

extern bool modo_diagnostico;

#endif
