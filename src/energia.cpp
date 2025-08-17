#include "energia.h"
#include "modem.h"
#include "gps.h"
#include "config.h"

void apagarComponentes() {
  Serial.println("🔌 Apagando periféricos...");
  modem.gprsDisconnect();
  powerOffGPS();
  // aquí podrías añadir apagado de sensores u otros módulos si los hubiera
}

void apagadoCritico() {
  apagarComponentes();
//  ESP.deepSleep(0);
}
