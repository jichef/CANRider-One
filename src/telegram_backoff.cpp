#include "telegram_backoff.h"
#include "config.h"   // APN, GPRS_USER, GPRS_PASS (si usas ensure_gprs_locked genérico)
#include "logs.h"

// ===== Estado interno =====
static uint16_t  s_fail_count     = 0;
static uint32_t  s_last_ok_ms     = 0;
static uint32_t  s_next_try_ms    = 0;
static uint8_t   s_backoff_shift  = 0; // crece con los fallos (tope 6)

// ===== Helpers =====
static inline uint32_t rng_jitter_250_1000() {
#ifdef ESP_RANDOM_H
  return 250 + (esp_random() % 751);
#else
  return 250 + (random(0, 751));
#endif
}

// Si prefieres usar tu connectGPRS() (recomendado), descomenta el return connectGPRS()
// y comenta el bloque “genérico con mutex” de abajo.
static bool ensure_gprs_locked() {
  if (gprsConnected) return true;

  // --- Vía tu función de proyecto (más limpia) ---
  // return connectGPRS();

  // --- Genérico con mutex (por si no usas connectGPRS()) ---
  bool took = (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(15000)) == pdTRUE);
  if (!took) return false;

  modem.gprsDisconnect();
  delay(300);
  bool ok = modem.gprsConnect(APN, GPRS_USER, GPRS_PASS);
  xSemaphoreGive(modemMutex);

  if (ok) gprsConnected = true;
  return ok;
}

static void soft_modem_restart_locked() {
  bool took = (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(20000)) == pdTRUE);
  if (!took) return;
  modem.gprsDisconnect();
  delay(200);
  modem.restart();      // reinicio lógico del SIM7000
  delay(1500);
  xSemaphoreGive(modemMutex);
  gprsConnected = false;
}

static void escalate_on(uint16_t fail_n) {
  if (fail_n == TG_FAILS_GPRS_RESET) {
    logEvento("tg_escalate_gprs_reset");
    (void)ensure_gprs_locked();
  } else if (fail_n == TG_FAILS_MODEM_RESTART) {
    logEvento("tg_escalate_modem_restart");
    soft_modem_restart_locked();
    (void)ensure_gprs_locked();
  } else if (fail_n == TG_FAILS_ESP_RESTART) {
    logEvento("tg_escalate_esp_restart");
    delay(250);
    ESP.restart();   // último recurso
  }
}

// ===== API =====
void telegram_backoff_reset() {
  s_fail_count = 0;
  s_backoff_shift = 0;
  s_next_try_ms = 0;
}

uint16_t telegram_fail_count()      { return s_fail_count; }
uint32_t telegram_last_ok_ms()      { return s_last_ok_ms; }
uint32_t telegram_next_try_eta_ms() { return s_next_try_ms; }

// Devuelve true si getMe() OK y deja tgReady=true. Respeta backoff y escalado.
bool telegram_init_step(uint32_t now_ms) {
  if (now_ms < s_next_try_ms) return false;

  // Si otro módulo usa el módem, aplaza 1s
  if (telegram_using_modem()) {
    s_next_try_ms = now_ms + 1000;
    return false;
  }

  // Asegurar GPRS
  if (!ensure_gprs_locked()) {
    s_fail_count++;
    escalate_on(s_fail_count);
    s_backoff_shift = (s_backoff_shift < 6) ? (uint8_t)(s_backoff_shift + 1) : 6;
    uint32_t wait_ms = min<uint32_t>(TG_BACKOFF_BASE_MS << s_backoff_shift, TG_BACKOFF_MAX_MS);
    s_next_try_ms = now_ms + wait_ms + rng_jitter_250_1000();
    return false;
  }

  // getMe() bajo mutex (HTTPS)
  bool took = (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(15000)) == pdTRUE);
  if (!took) {
    s_fail_count++;
    escalate_on(s_fail_count);
    s_backoff_shift = (s_backoff_shift < 6) ? (uint8_t)(s_backoff_shift + 1) : 6;
    uint32_t wait_ms = min<uint32_t>(TG_BACKOFF_BASE_MS << s_backoff_shift, TG_BACKOFF_MAX_MS);
    s_next_try_ms = now_ms + wait_ms + rng_jitter_250_1000();
    return false;
  }

  bool ok = false;
  secureClient.stop();
  secureClient.setTimeout(12000);
  ok = bot.getMe();

  xSemaphoreGive(modemMutex);

  if (ok && bot.userName.length() > 0) {
    // ÉXITO
    s_last_ok_ms = now_ms;
    s_fail_count = 0;
    s_backoff_shift = 0;
    s_next_try_ms = now_ms + 5000; // pequeña pausa antes del primer poll
    logEvento("tg_init_ok");
    tgReady = true;
    return true;
  }

  // FALLO
  s_fail_count++;
  logEvento("tg_init_fail");
  escalate_on(s_fail_count);

  // Si llevamos mucho sin éxito, cap menor
  bool stale = (s_last_ok_ms == 0) || (now_ms - s_last_ok_ms > TG_STALE_OK_MS);
  uint32_t max_cap = stale ? (TG_BACKOFF_MAX_MS / 2) : TG_BACKOFF_MAX_MS;

  s_backoff_shift = (s_backoff_shift < 6) ? (uint8_t)(s_backoff_shift + 1) : 6;
  uint32_t wait_ms = min<uint32_t>(TG_BACKOFF_BASE_MS << s_backoff_shift, max_cap);
  s_next_try_ms = now_ms + wait_ms + rng_jitter_250_1000();

  return false;
}
