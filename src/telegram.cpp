// telegram.cpp
#include <Arduino.h>
#include <string.h>
#include "config_user.h"                // BOT_TOKEN, CHAT_ID
#include "telegram.h"                   // prototipos públicos (si aplica)
#include "gps.h"                        // gpsActive(), gpsFixInProgress(), gpsStartFor(), gpsKill(), obtenerGPS()
#include "TinyGsmClientSIM7000SSL.h"
#include <UniversalTelegramBot.h>
#include "logs.h"                       // logMark/startWindow/endWindow

// --- FreeRTOS
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
}

// ================== EXTERNS DEL PROYECTO ==================
extern TinyGsmSim7000SSL modem;   // en modem.cpp
extern TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient; // en modem.cpp
extern UniversalTelegramBot bot;  // en modem.cpp
extern bool gprsConnected;        // en modem.cpp
extern SemaphoreHandle_t modemMutex;

// Reloj y batería
extern int localHour;
extern int minute;
extern uint8_t soc; // viene de bateria.h

// Conexión GPRS (si prefieres usar tu función de reconexión)
extern bool connectGPRS();

// ================== STUBS / WRAPPERS DE LINKER ==================
// Si otro módulo define versiones fuertes, el linker las usará.
// Si no, estas usan modemMutex como fallback seguro.
bool modemLock(const char* tag, uint32_t ms) __attribute__((weak));
void modemUnlock(const char* tag)          __attribute__((weak));

bool modemLock(const char* /*tag*/, uint32_t ms) {
  if (modemMutex) return xSemaphoreTake(modemMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
  return true; // sin mutex: no bloqueamos
}
void modemUnlock(const char* /*tag*/) {
  if (modemMutex) xSemaphoreGive(modemMutex);
}

// ================== GLOBALES ==================
volatile bool tgReady = false;         // <- definición
static volatile bool s_tgUsingModem = false;
static unsigned long lastPoll = 0;
static bool welcomeSent = false;

// Expuesto para otros módulos (p. ej., GPS arbitra el módem)
extern "C" bool telegram_using_modem() { return s_tgUsingModem; }

// ===== /gps asíncrono =====
static TaskHandle_t s_gpsTask = nullptr;
static volatile bool s_gpsCancel = false;
static uint32_t s_gpsCooldownMs = 0;   // pequeño enfriamiento tras /gps
static void GpsOpTask(void* arg);      // forward de la tarea

// ================== LOGGING LOCAL (sin colisiones) ==================
enum { TG_LOG_DEBUG = 0, TG_LOG_INFO = 1, TG_LOG_WARN = 2 };

static inline void logMsg(int lvl, const char* tag, const String& msg) {
  Serial.print("[");
  if (lvl == TG_LOG_DEBUG) Serial.print("DEBUG");
  else if (lvl == TG_LOG_INFO) Serial.print("INFO");
  else if (lvl == TG_LOG_WARN) Serial.print("WARN");
  else Serial.print("LOG");
  Serial.print("] ");
  Serial.print(tag);
  Serial.print(" — ");
  Serial.println(msg);
}

// ================== CONFIG ==================
#ifndef TG_POLL_PERIOD_MS
#define TG_POLL_PERIOD_MS 5000UL
#endif
#ifndef TG_SEND_TIMEOUT_MS
#define TG_SEND_TIMEOUT_MS 6000UL
#endif
#ifndef TG_SEND_RETRY_DELAY_MS
#define TG_SEND_RETRY_DELAY_MS 400UL
#endif
#ifndef GPS_ON_DURATION_MS
#define GPS_ON_DURATION_MS (10UL * 60UL * 1000UL) // 10 minutos
#endif

// ================== HELPERS ==================
static bool nameEquals(const char* a, const char* b) {
  if (!a || !b) return false;
  return strcmp(a, b) == 0;
}
static bool isTelegramTask() {
  const char* name = pcTaskGetName(nullptr);
  return nameEquals(name, "taskTelegram") || nameEquals(name, "TelegramTask");
}
static String hhmm() {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", localHour, minute);
  return String(buf);
}
static String gnssStatusLine() {
  if (gpsActive()) {
    if (gpsFixInProgress()) return "🛰️ GNSS: encendido (buscando FIX…)"; 
    return "🛰️ GNSS: encendido";
  }
  return "🛰️ GNSS: apagado";
}

// ================== ENVÍOS SEGUROS ==================
static bool reply(const String& chat_id, const String& msg) {
  logMsg(TG_LOG_DEBUG, "TG_SEND", String("to=chat_id len=") + msg.length());
  if (!isTelegramTask() && !tgReady) return false;

  const uint32_t lock_ms = isTelegramTask() ? 1500 : 0;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!modemLock("TG_SEND", lock_ms)) {
      if (!isTelegramTask()) return false;
      vTaskDelay(pdMS_TO_TICKS(TG_SEND_RETRY_DELAY_MS));
      continue;
    }
    s_tgUsingModem = true;
    secureClient.stop();
    secureClient.setTimeout(TG_SEND_TIMEOUT_MS);
    bool ok = bot.sendMessage(chat_id, msg, "");
    s_tgUsingModem = false;
    modemUnlock("TG_SEND");

    logMsg(ok ? TG_LOG_INFO : TG_LOG_WARN, "TG_SEND", ok ? "OK (Q)" : "FAIL (Q)");
    if (ok) return true;
    vTaskDelay(pdMS_TO_TICKS(TG_SEND_RETRY_DELAY_MS));
  }
  return false;
}

static bool replyMd(const String& chat_id, const String& msg) {
  logMsg(TG_LOG_DEBUG, "TG_SEND", String("to=chat_id len=") + msg.length() + " (MD)");
  if (!isTelegramTask() && !tgReady) return false;

  const uint32_t lock_ms = isTelegramTask() ? 1500 : 0;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!modemLock("TG_SEND", lock_ms)) {
      if (!isTelegramTask()) return false;
      vTaskDelay(pdMS_TO_TICKS(TG_SEND_RETRY_DELAY_MS));
      continue;
    }
    s_tgUsingModem = true;
    secureClient.stop();
    secureClient.setTimeout(TG_SEND_TIMEOUT_MS);
    bool ok = bot.sendMessage(chat_id, msg, "Markdown");
    s_tgUsingModem = false;
    modemUnlock("TG_SEND");

    logMsg(ok ? TG_LOG_INFO : TG_LOG_WARN, "TG_SEND", ok ? "OK (MD)" : "FAIL (MD)");
    if (ok) return true;
    vTaskDelay(pdMS_TO_TICKS(TG_SEND_RETRY_DELAY_MS));
  }
  return false;
}

// ================== FLUSH RÁPIDO (avanza offset) ==================
bool telegramFlushQuick(uint16_t timeout_ms) {
  if (!tgReady) return false;
  if (!modemLock("TG_FLUSH", 1200)) return false;
  s_tgUsingModem = true;
  secureClient.setTimeout(timeout_ms);
  (void)bot.getUpdates(bot.last_message_received + 1); // avanza offset
  s_tgUsingModem = false;
  modemUnlock("TG_FLUSH");
  return true;
}

// ================== WELCOME ==================
bool sendWelcomeOnce() {
  if (welcomeSent) return true;
  String msg;
  msg.reserve(240);
  msg += "🤖 CANRIDER iniciado a las ";
  msg += hhmm();
  msg += "\n\nComandos:\n";
  msg += "• 🛠️ /estado — Estado del sistema y GPRS\n";
  msg += "• 🛰️ /gps — Activa el GNSS 10 min\n";
  msg += "• ✋ /gpskill — Apaga GNSS / cancela intento\n";
  msg += "• 🔁 /reboot — Reinicia el dispositivo\n";
  bool ok = reply(String(CHAT_ID), msg);
  if (ok) welcomeSent = true;
  return ok;
}

// ================== INIT + BACKOFF ==================
#include "telegram_backoff.h"

// Fachada mínima para mantener tus logs/estructura
bool initTelegram() {
  logMark("TELEGRAM_INIT");   // <- viene de logs.h
  return telegram_init_step(millis());
}

bool ensureTelegramReadyWithBackoff() {
  if (tgReady) return true;
  (void)initTelegram();
  return tgReady;
}

// ================== /gps: worker & progreso ==================
using GpsProgressCb = void(*)(void*, uint16_t, uint16_t, int, int);

// Helper de rate-limit (cerca de reply/replyMd)
static inline bool replyRateLimited(const String& chat_id, const String& msg, bool md=false) {
  static uint32_t lastSendMs = 0, windowMs = 0;
  static uint16_t sentThisMin = 0;
  uint32_t now = millis();
  if (now - windowMs > 60000UL) { windowMs = now; sentThisMin = 0; }
  if (now - lastSendMs < 2000UL) return false;   // ≥2 s entre mensajes
  if (sentThisMin >= 20)         return false;   // ≤20/min
  bool ok = md ? replyMd(chat_id, msg) : reply(chat_id, msg);
  if (ok) { lastSendMs = now; sentThisMin++; }
  return ok;
}

// Progreso GNSS: no spamear y respetar cancelación
static void tgGpsProgress(void* /*ctx*/, uint16_t elapsed_s, uint16_t total_s, int used, int view) {
  static unsigned long lastNotify = 0;
  if (s_gpsCancel) return;

  unsigned long now = millis();
  if (now - lastNotify < 30000UL) return; // como mucho 1 msg/30s
  lastNotify = now;

  String msg = "🛰️ Buscando FIX… " + String(elapsed_s) + "/" + String(total_s) + " s";
  if (view >= 0 || used >= 0) {
    msg += "\n👀 En vista: " + String(view >= 0 ? view : 0);
    msg += " | Usados: " + String(used >= 0 ? used : 0);
  }

  (void)replyRateLimited(String(CHAT_ID), msg, /*md=*/false);
}

// Tarea separada para /gps (permite /gpskill sin bloqueo)
static void GpsOpTask(void* /*arg*/) {
  // Evitar relanzar /gps enseguida tras terminar
  s_gpsCooldownMs = millis() + 8000UL; // 8 s de cooldown

  // Encender GNSS por la duración objetivo
  bool on = gpsStartFor(GPS_ON_DURATION_MS);
  if (!on) {
    reply(String(CHAT_ID), "⚠️ No se pudo activar el GNSS (módulo ocupado o no disponible).");
    s_gpsTask = nullptr;
    s_gpsCancel = false;   // limpiar cancelación para próximas ejecuciones
    vTaskDelete(nullptr);
    return;
  }

  // Obtener posición con progreso
  String resp;
  bool got = obtenerGPS(resp, GPS_ON_DURATION_MS, tgGpsProgress, nullptr, 30);

  // Si no se ha cancelado, reportar resultado
  if (!s_gpsCancel) {
    if (resp.length()) reply(String(CHAT_ID), resp);
    if (!got)          reply(String(CHAT_ID), "❌ No fue posible obtener la ubicación en el tiempo previsto.");
    (void)telegramFlushQuick(1200);
  }

  // Log de stack
  UBaseType_t hw = uxTaskGetStackHighWaterMark(nullptr);
  Serial.printf("[GpsOpTask] stack HWM: %u bytes\n", (unsigned)hw);

  // Fin de la tarea
  s_gpsCancel = false;
  s_gpsTask = nullptr;
  vTaskDelete(nullptr);
}

// ================== COMANDOS ==================
static void handleCommand(const String& cmd, const String& chat_id, uint32_t chat_u32 = 0) {
  (void)chat_u32;

  if (cmd == "/start") {
    String msg;
    msg.reserve(200);
    msg  = "🤖 Hola. Comandos:\n";
    msg += "• 🛠️ /estado — estado del sistema y GPRS\n";
    msg += "• 🛰️ /gps — activa el GNSS 10 min\n";
    msg += "• ✋ /gpskill — apaga GNSS / cancela intento\n";
    msg += "• 🔁 /reboot — reinicia el dispositivo\n";
    reply(chat_id, msg);

  } else if (cmd == "/estado") {
    String msg = "⚡ *Estado del sistema*\n";
    if (soc <= 100) {
      int filled = (soc + 9) / 10;
      String bar = "[";
      for (int i = 0; i < 10; ++i) bar += (i < filled ? "█" : "░");
      bar += "]";
      msg += "🔋 Batería: " + String((int)soc) + "%\n" + bar + "\n";
    } else {
      msg += "🔋 SoC desconocido (aún no recibido por CAN 0x541)\n";
    }
    msg += "⏰ Hora: " + hhmm() + "\n";
    msg += gnssStatusLine();
    replyMd(chat_id, msg);

  } else if (cmd == "/gps") {
    // Evita relanzar durante el cooldown
    if (millis() < s_gpsCooldownMs) {
      reply(chat_id, "⏳ Espera unos segundos antes de volver a /gps.");
      return;
    }

    if (s_gpsTask) {
      reply(chat_id, "⏳ Ya hay un /gps en curso. Usa /gpskill para cancelarlo.");
    } else {
      s_gpsCancel = false;
      BaseType_t ok = xTaskCreatePinnedToCore(
        GpsOpTask, "GpsOpTask",
        10240,              // (o 12288 si prefieres más margen)
        nullptr,
        1,                  // prioridad baja-media
        &s_gpsTask,
        1                   // pin a CORE 1
      );
      if (ok == pdPASS) {
        reply(chat_id, "🛰️ GNSS activado por 10 minutos.\n⏳ Intentando fijar posición…");
        reply(chat_id, "🎯 Refinando precisión — objetivo *Excelente* (HDOP ≤ 1.2, usados ≥ 9)");
      } else {
        s_gpsTask = nullptr;
        reply(chat_id, "❌ No se pudo crear la tarea GNSS.");
      }
    }

  } else if (cmd == "/gpskill") {
    if (!s_gpsTask) {
      reply(chat_id, "ℹ️ No hay un /gps en curso.");
    } else {
      s_gpsCancel = true;   // marca cancelación
      gpsKill();            // apaga GNSS si tu driver lo soporta

      // Espera breve a que la tarea salga sola
      uint32_t t0 = millis();
      while (s_gpsTask && (millis() - t0) < 1500UL) {
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      if (s_gpsTask) {
        // Sigue viva: cortacircuito
        vTaskDelete(s_gpsTask);
        s_gpsTask = nullptr;
      }
      reply(chat_id, "✋ GNSS detenido.");
      (void)telegramFlushQuick(1200);
      s_gpsCancel = false; 
    }

  } else if (cmd == "/reboot" || cmd == "/restart") {
    reply(chat_id, "🔁 Reiniciando en 30 s… (Telegram puede tardar >2 min en volver)");
    vTaskDelay(pdMS_TO_TICKS(800));
    if (modemLock("TG_FLUSH", 1200)) {
      s_tgUsingModem = true;
      secureClient.setTimeout(3000);
      (void)bot.getUpdates(bot.last_message_received + 1); // avanza offset
      s_tgUsingModem = false;
      modemUnlock("TG_FLUSH");
    }
    vTaskDelay(pdMS_TO_TICKS(1200));
    ESP.restart();

  } else {
    reply(chat_id, "🤖 Comando no reconocido.");
  }
}

// Coincidir con el .h (no static)
bool chatIDAutorizado(const String& chat_id) {
  if (String(CHAT_ID).length() == 0) return true; // sin control de ACL
  return chat_id == String(CHAT_ID);
}

void processCommand(const String& command, const String& chat_id) {
  if (!chatIDAutorizado(chat_id)) {
    reply(chat_id, "🚫 No autorizado.");
    return;
  }
  handleCommand(command, chat_id);
}

// ================== POLL CON ARBITRAJE GNSS ==================
// Devuelve nº de mensajes nuevos (>=0) o -1 si error
int checkTelegram(unsigned long now) {
  // Asegurar bot ready con backoff/escalado
  (void)ensureTelegramReadyWithBackoff();
  if (!gprsConnected || !tgReady) return 0;

  // Saltar polls si FIX GNSS está en curso (priorizar GNSS sobre TLS HTTP)
  bool busy = gpsFixInProgress();
  static uint8_t s_skip = 0;
  if (busy) {
    s_skip = (s_skip + 1) % 3;
    if (s_skip != 0) {
      logMsg(TG_LOG_DEBUG, "TG_POLL", "saltado (prioridad GNSS)");
      return 0;
    }
  }

  // long-poll adaptativo
  bot.longPoll = busy ? 0 : 10;

  if (now - lastPoll < TG_POLL_PERIOD_MS) return 0;
  lastPoll = now;

  startWindow("TELEGRAM_POLL");   // <- de logs.h

  if (!modemLock("TG_POLL", 1500)) {
    logMsg(TG_LOG_WARN, "MUTEX", "NO lock tag=TG_POLL");
    endWindow("TELEGRAM_POLL");
    return 0;
  }

  s_tgUsingModem = true;
  secureClient.setTimeout(busy ? 4000 : 12000);
  int n = bot.getUpdates(bot.last_message_received + 1);
  s_tgUsingModem = false;
  modemUnlock("TG_POLL");

  logMsg(TG_LOG_INFO, "TG_POLL", String("n=") + n + " last_id=" + bot.last_message_received + (busy ? " (LP=0)" : " (LP=10)"));

  if (n <= 0) {
    endWindow("TELEGRAM_POLL");
    return n; // 0 = sin mensajes, -1 = error interno de la lib
  }

  for (int i = 0; i < n; i++) {
    const String chat_id = bot.messages[i].chat_id;
    const String text    = bot.messages[i].text;
    logMsg(TG_LOG_DEBUG, "TG_MSG", String("from=? text=") + text); // ocultamos chat_id en logs

    if (!chatIDAutorizado(chat_id)) {
      reply(chat_id, "🚫 No autorizado.");
      continue;
    }
    handleCommand(text, chat_id);
  }

  endWindow("TELEGRAM_POLL");     // <- de logs.h
  return n;
}

// ================== API UTIL PARA OTROS MÓDULOS ==================
bool telegramSend(const String& msg) {
  return reply(String(CHAT_ID), msg);
}
