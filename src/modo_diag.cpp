#include "modo_diag.h"

bool modo_diagnostico = false;

void handleModoDiagnostico() {
  // Aquí podrías encender un LED o mostrar algo por consola
  if (modo_diagnostico) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
      Serial.println("🧪 Modo diagnóstico activo");
      lastPrint = millis();
    }
  }
}

bool modoDiagnosticoActivo() {
  return modo_diagnostico;
}

void activarModoDiagnostico() {
  modo_diagnostico = true;
}
