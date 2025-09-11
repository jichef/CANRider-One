#include "bateria.h"
#include "config.h"
#include "energia.h"
#include "telegram.h"
#include "logs.h"
#include <Arduino.h>

// ===================== Config robustez SoC =====================
// Si no llega SoC reciente, usamos este comodín para no parar el sistema
#ifndef SOC_PLACEHOLDER
#define SOC_PLACEHOLDER 55
#endif

// Cuánto tiempo consideramos "reciente" el último SoC (ms)
#ifndef SOC_STALE_MS
#define SOC_STALE_MS (5UL * 60UL * 1000UL)  // 5 minutos
#endif

// Nº de tramas consecutivas a 0 para aceptar 0% como real
#ifndef SOC_ZERO_CONFIRM_N
#define SOC_ZERO_CONFIRM_N 3
#endif

// Marcador de desconocido
#ifndef SOC_UNKNOWN
#define SOC_UNKNOWN 255
#endif

// ===================== Estado SoC =====================
uint8_t soc = 0;           // SoC crudo (tal como te llega de CAN 0x541 data[0])
static volatile uint8_t  s_soc_raw     = SOC_UNKNOWN;
static volatile uint32_t s_soc_last_ms = 0;                 // millis() de la última trama
static uint8_t           s_soc_last_ok = SOC_PLACEHOLDER;   // último >0 aceptado
static uint8_t           s_zero_streak = 0;                 // racha de ceros consecutivos

bool apagado_ya = false;

// ===================== Ingesta desde CAN =====================
// Llama a esto desde tu callback cuando recibas ID 0x541 (data[0] = SoC)
void battery_onCan541(const uint8_t* data, uint8_t len) {
  if (!data || len == 0) return;
  const uint8_t v = data[0];

  soc          = v;          // mantenemos compatibilidad con tu global existente
  s_soc_raw    = v;
  s_soc_last_ms = millis();

  if (v == 0) {
    if (s_zero_streak < 250) s_zero_streak++;
  } else {
    s_zero_streak = 0;
    if (v != SOC_UNKNOWN && v <= 100) {
      if (v > 0) s_soc_last_ok = v;   // sólo guardamos >0 como "bueno"
    }
  }
}

// ===================== Lecturas robustas =====================
uint8_t soc_effective() {
  const uint32_t now = millis();
  const bool stale = (s_soc_last_ms == 0) || (now - s_soc_last_ms > SOC_STALE_MS);

  if (stale) {
    // Dato viejo o nunca recibido → comodín
    return SOC_PLACEHOLDER;
  }

  if (s_soc_raw == 0) {
    // Solo aceptamos 0 real si hay racha confirmada
    if (s_zero_streak >= SOC_ZERO_CONFIRM_N) return 0;
    return s_soc_last_ok;  // 0 dudoso → mantenemos el último bueno
  }

  if (s_soc_raw == SOC_UNKNOWN) {
    // Mantén el último bueno o comodín
    return (s_soc_last_ok != 0 ? s_soc_last_ok : SOC_PLACEHOLDER);
  }

  // Valor 1..100 reciente
  return s_soc_raw;
}

// Crítico real: 0% confirmado y dato no caduco
bool soc_critico_confirmado() {
  const uint32_t now = millis();
  const bool fresh = (s_soc_last_ms != 0) && (now - s_soc_last_ms <= SOC_STALE_MS);
  return fresh && (s_soc_raw == 0) && (s_zero_streak >= SOC_ZERO_CONFIRM_N);
}

// ===================== Lógica principal =====================
void checkSoC() {
  if (modo_diagnostico) return;

  const uint8_t eff = soc_effective();

  // Apagado crítico SOLO si 0 confirmado (evita falsos 0 por CAN caído)
  if (!apagado_ya && soc_critico_confirmado()) {
    const String msg = "🔌 Apagado crítico por batería baja (" + String(eff) + "%).";
    telegramSend(msg);
    logEvento("apagado_critico");
    apagarComponentes();
    apagado_ya = true;
    delay(500);
    // ESP.deepSleep(0);
    return;
  }

  // ⚠️ Avisos de alarma se basan en el efectivo (no en el crudo)
  static bool alerta_enviada_interna = false;

  if (eff < SOC_ALARMA && !alerta_enviada_interna) {
    telegramSend("⚠️ Batería baja: " + String(eff) + "%");
    alerta_enviada_interna = true;
  }

  if (eff >= (SOC_ALARMA + 5)) {
    alerta_enviada_interna = false;
  }
}

void checkSoCOnBoot() {
  // Pequeña espera por estabilidad de alimentación
  delay(3000);

  if (modo_diagnostico) return;

  // En arranque, también respetamos la confirmación de 0 real
  if (soc_critico_confirmado()) {
    Serial.println("⚠️ Apagando en setup por SoC crítico confirmado");
    apagarComponentes();
    // ESP.deepSleep(0);
  }
}
