// telegram.cpp — ESP32 + SIM7000SSL + UniversalTelegramBot
// - Priming TLS desde la task de Telegram
// - Envíos fuera de la task con try-lock (no bloqueante)
// - Wrapper sendTelegramMessage() para compatibilidad con bateria.cpp
// - Stubs weak de modemLock/modemUnlock (si no existen los reales)
// - Persistencia robusta del offset en NVS (Preferences) para evitar bucles con /reboot

#include <Arduino.h>
#include <string.h>
#include <Preferences.h>              // NVS para persistir last update_id

#include "config_user.h"              // BOT_TOKEN, CHAT_ID
#include "telegram.h"

#include "TinyGsmClientSIM7000SSL.h"
#include <UniversalTelegramBot.h>

// --- FreeRTOS (rutas correctas en ESP32 Arduino)
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
}

// ======== Limites de red para no bloquear el módem ========
#ifndef TG_NET_BUDGET_MS
  #define TG_NET_BUDGET_MS   2500UL   // presupuesto por operación Telegram
#endif
#ifndef TG_HTTP_TIMEOUT_MS
  #define TG_HTTP_TIMEOUT_MS 2000UL   // timeout del socket HTTPS
#endif

// ================== EXTERNS DEL PROYECTO ==================
extern TinyGsmSim7000SSL modem;
extern TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient;

extern bool gprsConnected;
extern SemaphoreHandle_t modemMutex;

extern int localHour;
extern int minute;
extern uint8_t soc;  // viene de bateria.h

// Declaración adelantada del manejador de comandos (si existe en este .cpp)
static void handleCommand(const String& cmd, const String& chat_id);

// Try-lock con timeout corto + tag opcional para traza
static inline bool modemTryLock(uint32_t ms, const char* tag) {
  (void)tag; // solo para trazas si quisieras
  if (!modemMutex) return true;
  return xSemaphoreTake(modemMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
// ================== GLOBALES ==================
bool welcomeSent = false;  // única definición

// ---- NVS (Preferences) para last update_id ----
static Preferences tgPrefs;
static bool tgPrefsOpen = false;

static inline void tgPrefsBegin() {
  if (!tgPrefsOpen) { tgPrefs.begin("telegram", false); tgPrefsOpen = true; }
}
static inline void tgPrefsEnd() {
  if (tgPrefsOpen) { tgPrefs.end(); tgPrefsOpen = false; }
}
static inline uint32_t tgLoadLastUpdateFromNVS() {
  tgPrefsBegin();
  return tgPrefs.getUInt("last_id", 0);
}
static inline void tgPersistLastUpdateToNVS(uint32_t id) {
  tgPrefsBegin();
  tgPrefs.putUInt("last_id", id);
}

// ================== STUBS / WRAPPERS DE LINKER ==================
bool modemLock(const char* tag, uint32_t ms) __attribute__((weak));
void modemUnlock(const char* tag)           __attribute__((weak));

bool modemLock(const char* tag, uint32_t ms) {
  (void)tag;
  if (modemMutex) {
    if (xSemaphoreTake(modemMutex, pdMS_TO_TICKS(ms)) == pdTRUE) return true;
    return false;
  }
  return true;  // sin mutex, no bloqueamos
}
void modemUnlock(const char* tag) {
  (void)tag;
  if (modemMutex) xSemaphoreGive(modemMutex);
}

// --- Wrapper para compatibilidad con bateria.cpp ---
bool telegramSend(const String& msg); // declarado en telegram.h
void sendTelegramMessage(const String& msg) { (void)telegramSend(msg); }

// ================== LOGGING SHIMS (opcionales) ==================
#ifndef LOG_DEBUG
  #define LOG_DEBUG 0
#endif
#ifndef LOG_INFO
  #define LOG_INFO  1
#endif
#ifndef LOG_WARN
  #define LOG_WARN  2
#endif

static inline void logMsg(int lvl, const char* tag, const String& msg) {
  Serial.print("[");
  if (lvl == LOG_DEBUG) Serial.print("DEBUG");
  else if (lvl == LOG_INFO) Serial.print("INFO");
  else if (lvl == LOG_WARN) Serial.print("WARN");
  else Serial.print("LOG");
  Serial.print("] ");
  Serial.print(tag);
  Serial.print(" — ");
  Serial.println(msg);
}

static inline void logMark(const char* mark) {
  Serial.println();
  Serial.println("==================================================");
  Serial.print("== ");
  Serial.print(mark);
  Serial.println(" ==");
  Serial.println("==================================================");
}

static inline void startWindow(const char* name) { (void)name; }
static inline void endWindow(const char* name)   { (void)name; }

// ================== TELEGRAM BOT ==================
static UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// Config
#ifndef TG_POLL_PERIOD_MS
  #define TG_POLL_PERIOD_MS  3000UL
#endif
#ifndef TG_SEND_TIMEOUT_MS
  #define TG_SEND_TIMEOUT_MS 6000UL
#endif
#ifndef TG_SEND_RETRY_DELAY_MS
  #define TG_SEND_RETRY_DELAY_MS 400UL
#endif

// Estado interno
static volatile bool tgReady = false; // tras priming TLS hecho por la task de Telegram
static unsigned long lastPoll = 0;

// ================== HELPERS ==================
static bool nameEquals(const char* a, const char* b) {
  if (!a || !b) return false;
  return strcmp(a, b) == 0;
}

// Detecta la tarea por nombre (acepta "taskTelegram" o "TelegramTask")
static bool isTelegramTask() {
  const char* name = pcTaskGetName(nullptr);
  return nameEquals(name, "taskTelegram") || nameEquals(name, "TelegramTask");
}

// Envío seguro: fuera de la tarea => try-lock (no bloqueante)
static bool reply(const String& chat_id, const String& msg) {
  logMsg(LOG_DEBUG, "TG_SEND", String("to=") + chat_id + " len=" + msg.length());

  // Si NO somos la task de Telegram y aún no hay priming TLS, no intentamos
  if (!isTelegramTask() && !tgReady) return false;

  // try-lock: si el módem está ocupado, saltamos sin bloquear
  if (!modemLock("TG_SEND", 0)) {
    logMsg(LOG_DEBUG, "TG_SEND", "skip: modem busy");
    return false;
  }

  secureClient.stop();
  secureClient.setTimeout(TG_HTTP_TIMEOUT_MS);

  bool ok = bot.sendMessage(chat_id, msg, "");  // sin Markdown
  modemUnlock("TG_SEND");

  logMsg(ok ? LOG_INFO : LOG_WARN, "TG_SEND", ok ? "OK" : "FAIL");
  return ok;
}

bool sendWelcomeOnce() {
  if (welcomeSent) return true;

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", localHour, minute);

  String msg;
  msg.reserve(240);
  msg += F("🤖 CANRIDER listo (");
  msg += hhmm;
  msg += F(")\n\n");
  msg += F("• ⏰ /hora — hora actual\n");
  msg += F("• 🔋 /bateria — nivel de batería (SoC)\n");
  msg += F("• 🛠️ /estado — estado del sistema y GPRS\n");
  msg += F("• 🛰️ /gps — ubicación (activa GPS temporalmente)\n");
  msg += F("• 🔁 /reboot — reinicia el dispositivo\n");

  bool ok = telegramSend(msg);
  if (ok) welcomeSent = true;
  return ok;
}

// ================== API PÚBLICA ==================

// Handshake TLS desde la task de Telegram (sin saludo aquí)
bool initTelegram() {
  bot.longPoll = 15;   // evitar bloqueos largos en getUpdates()

  logMark("TELEGRAM_INIT");
  vTaskDelay(pdMS_TO_TICKS(500));  // warm-up

  if (!gprsConnected) {
    logMsg(LOG_WARN, "TELEGRAM", "Sin GPRS en init; difiriendo");
    return false;
  }

  // Primer handshake dentro de la tarea de Telegram
  if (modemLock("TG_PRIME", 1200)) {
    secureClient.stop();
    secureClient.setTimeout(12000);
    (void)bot.getMe();      // abre sesión TLS; el resultado no es crítico
    modemUnlock("TG_PRIME");
    tgReady = true;

    // Restaura offset procesado antes del último reinicio desde NVS
    bot.last_message_received = tgLoadLastUpdateFromNVS();
  } else {
    logMsg(LOG_WARN, "MUTEX", "NO lock tag=TG_PRIME (init)");
    return false;
  }

  // No mandar bienvenida aquí
  return true;
}

// Procesamiento de comandos
static void handleCommand(const String& cmd, const String& chat_id) {
  if (cmd == "/start") {
    String msg;
    msg.reserve(240);
    msg += F("🤖 Hola. Comandos:\n");
    msg += F("• ⏰ /hora — hora actual\n");
    msg += F("• 🔋 /bateria — nivel de batería (SoC)\n");
    msg += F("• 🛠️ /estado — estado del sistema y GPRS\n");
    msg += F("• 🛰️ /gps — activa el GPS y envía ubicación\n");
    msg += F("• 🔁 /reboot — reinicia el dispositivo\n");
    reply(chat_id, msg);
  } else if (cmd == "/net") {
    String s;
    s.reserve(160);
    s += "🌐 NET\n";
    s += "• tgReady: "; s += (tgReady ? "1" : "0"); s += "\n";
    s += "• gprs: ";    s += (gprsConnected ? "1" : "0"); s += "\n";
    s += "• last_id: "; s += String(bot.last_message_received); s += "\n";
    s += "• NVS last: "; s += String(tgLoadLastUpdateFromNVS());
    reply(chat_id, s);

  } else if (cmd == "/hora") {
    char buf[32];
    snprintf(buf, sizeof(buf), "🕒 %02d:%02d", localHour, minute);
    reply(chat_id, String(buf));

  } else if (cmd == "/estado") {
    reply(chat_id, String("🔧 Sistema OK. GPRS ") + (gprsConnected ? "✅" : "❌"));

  } else if (cmd == "/gps") {
    reply(chat_id, "🛰️ Activando GPS…");   // tu GPSTask actuará

  } else if (cmd == "/bateria") {
    if (soc <= 100) {
      int filled = (soc + 9) / 10;   // redondeo arriba
      String bar = "[";
      for (int i = 0; i < 10; ++i) bar += (i < filled ? "█" : "░");
      bar += "]";
      char buf[48];
      snprintf(buf, sizeof(buf), "🔋 Batería: %u%%", soc);
      reply(chat_id, String(buf) + "\n" + bar);
    } else {
      reply(chat_id, "🔋 SoC desconocido (aún no recibido por CAN 0x541).");
    }

  } else if (cmd == "/reboot" || cmd == "/restart") {
    // La persistencia del update_id se asegura en checkTelegram antes de llegar aquí
    reply(chat_id, "🔁 Reiniciando en 2 s…");
    vTaskDelay(pdMS_TO_TICKS(1500));   // margen para que el mensaje salga
    tgPrefsEnd();                      // cerrar NVS limpio (opcional)
    ESP.restart();

  } else {
    reply(chat_id, "🤖 Comando no reconocido.");
  }
}

// Implementaciones opcionales expuestas en el .h
bool chatIDAutorizado(const String& chat_id) {
  if (String(CHAT_ID).length() == 0) return true;   // sin whitelist, permitir
  return chat_id == String(CHAT_ID);
}

void processCommand(const String& command, const String& chat_id) {
  if (!chatIDAutorizado(chat_id)) {
    reply(chat_id, "🚫 No autorizado.");
    return;
  }
  handleCommand(command, chat_id);
}

// ================== POLL DE TELEGRAM (NO BLOQUEANTE) ==================
int checkTelegram(unsigned long now) {
  if (!tgReady) return 0;
  if (now - lastPoll < TG_POLL_PERIOD_MS) return 0;
  lastPoll = now;

  // Si GPRS está abajo, sonda breve cada 60 s y salimos sin bloquear
  if (!gprsConnected) {
    static unsigned long lastProbe = 0;
    if (now - lastProbe >= 60000UL) {
      lastProbe = now;

      if (modemLock("TG_PROBE", 0)) {       // try-lock: no bloquea
        secureClient.stop();
        secureClient.setTimeout(1200);      // sonda corta
        bool ok = bot.getMe();              // prueba TLS a Telegram
        modemUnlock("TG_PROBE");

        logMsg(ok ? LOG_INFO : LOG_WARN, "TG_PROBE", ok ? "OK" : "FAIL");
        if (ok) {
          // La conexión de datos realmente funciona: levanta la flag
          gprsConnected = true;
        }
      }
    }
    return 0;  // dejar que el watchdog/redial haga su trabajo
  }

  startWindow("TELEGRAM_POLL");

  // Poll protegido con lock propio (try-lock: no bloquea al resto)
  if (!modemLock("TG_POLL", 0)) {
    logMsg(LOG_DEBUG, "TG_POLL", "skip: modem busy");
    endWindow("TELEGRAM_POLL");
    return 0;
  }

  secureClient.setTimeout(TG_HTTP_TIMEOUT_MS);
  unsigned long t0 = millis();
  int n = bot.getUpdates(bot.last_message_received + 1);
  unsigned long dt = millis() - t0;
  modemUnlock("TG_POLL");

  if (dt > TG_NET_BUDGET_MS) {
    logMsg(LOG_WARN, "TG_POLL", String("getUpdates lento: ") + dt + " ms");
  }

  if (n <= 0) {
    endWindow("TELEGRAM_POLL");
    return n;
  }

  uint32_t max_id = 0;  // track del último update_id procesado

  for (int i = 0; i < n; i++) {
    const String chat_id = bot.messages[i].chat_id;
    const String text    = bot.messages[i].text;

    // update_id es int en esta lib
    uint32_t uid = (uint32_t) bot.messages[i].update_id;
    if (uid > max_id) max_id = uid;

    if (!chatIDAutorizado(chat_id)) {
      reply(chat_id, "🚫 No autorizado.");
      continue;
    }

    // Persistir ANTES de ejecutar /reboot para romper bucles tras reinicio
    if (text == "/reboot" || text == "/restart") {
      bot.last_message_received = uid;
      tgPersistLastUpdateToNVS(uid);   // guarda en NVS antes de reiniciar
    }

    handleCommand(text, chat_id);
  }

  // Tras procesar, fija offset para próximas consultas y para el próximo arranque
  if (max_id > 0) {
    bot.last_message_received = max_id;
    tgPersistLastUpdateToNVS(max_id);  // persistente incluso tras power-cycle
  }

  endWindow("TELEGRAM_POLL");
  return n;
}



bool telegramSend(const String& msg) {
  return reply(String(CHAT_ID), msg);
}


bool telegramPollOnce() {
  // Ventana corta para no “atar” el módem
  const uint32_t TRYLOCK_MS   = 25;
  const uint32_t WARN_MS      = 18000;   // avisa si tarda > 18 s

  // Si el módem está ocupado, NO bloquees
  if (!modemTryLock(TRYLOCK_MS, "TG_POLL")) {
    Serial.println(F("TG_POLL — skip: modem busy"));
    return false;
  }

  // Ajustes de long-poll y timeout de lectura
  bot.longPoll = 15; // segundos (reduce contención)
  secureClient.setTimeout(12000); // ms

  uint32_t t0 = millis();
  int updates = bot.getUpdates(bot.last_message_received + 1);
  uint32_t dt = millis() - t0;

  if (dt > WARN_MS) {
    Serial.printf("[WARN] TG_POLL — getUpdates lento: %lu ms\n", (unsigned long)dt);
  }

  // Suelta el módem ANTES de procesar
  modemUnlock("TG_POLL");

  if (updates <= 0) {
    // -1 = error; 0 = sin mensajes; ambos ya sin monopolizar el módem
    return updates == 0 ? true : false;
  }

  // Procesa mensajes (tu handleCommand debería hacer sus propios locks al enviar)
  for (int i = 0; i < updates; i++) {
    const String &cmd     = bot.messages[i].text;
    const String &chat_id = bot.messages[i].chat_id;
    handleCommand(cmd, chat_id);
  }

  // Pequeño respiro para que otras tasks tomen el módem
  vTaskDelay(pdMS_TO_TICKS(1200));
  return true;
}

