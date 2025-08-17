#include "bateria.h"
#include "config.h"
#include "energia.h"
#include "telegram.h"
#include "logs.h"

uint8_t soc = 0;  
bool apagado_ya = false;

void checkSoC() {
  if (modo_diagnostico) return;

  if (soc < SOC_CRITICO && !apagado_ya) {
    sendTelegramMessage("🔌 Apagado crítico por batería baja (" + String(soc) + "%).");
    logEvento("apagado_critico");
    apagarComponentes();
    apagado_ya = true;
    delay(500);
  //  ESP.deepSleep(0);
  }

  static bool alerta_enviada_interna = false;
  if (soc < SOC_ALARMA && !alerta_enviada_interna) {
    sendTelegramMessage("⚠️ Batería baja: " + String(soc) + "%");
    alerta_enviada_interna = true;
  }

  if (soc >= (SOC_ALARMA + 5)) {
    alerta_enviada_interna = false;
  }
}

void checkSoCOnBoot() {
  delay(3000);  // espera inicial por si la alimentación aún se estabiliza
  if (soc < SOC_CRITICO && !modo_diagnostico) {
    Serial.println("⚠️ Apagando en setup por SoC crítico");
    apagarComponentes();
 //   ESP.deepSleep(0);
  }
}
