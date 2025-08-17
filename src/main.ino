#include <Arduino.h>
#include "config.h"
#include "config_user.h"

#include "can_bus.h"
#include "modem.h"
#include "gps.h"
#include "telegram.h"
#include "bateria.h"
#include "energia.h"
#include "logs.h"        // <<— tu logger (log.cpp)
#include "modo_diag.h"
#include "utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ========= Variables globales =========
int localHour = 0;
int minute = 0;
time_t hora_actual;
bool hora_valida = false;
extern bool gprsConnected;
int telegramErrorCount = 0;

// Mutex para proteger el módem (AT/HTTPS/GPS/Telegram)
SemaphoreHandle_t modemMutex = nullptr;

// Centinela para "SoC desconocido" (soc es uint8_t en bateria.h)
static const uint8_t SOC_UNKNOWN = 0xFF;

// ========= Prototipos de tareas locales =========
void taskTelegram(void *pvParameters);
void taskGPS(void *pvParameters);

extern TinyGsmSim7000SSL modem;
bool gprsConnected = false;
uint32_t failStreak = 0;


// ========= setup =========
void setup() {
  Serial.begin(115200);
  delay(500);

  // --- LOGGER ---
  initLogger(Serial, LOG_DEBUG);
  logMark("SETUP_BEGIN");
  logMsg(LOG_INFO, "SETUP", "Iniciando sistema...");

  // === CAN primero ===
  initCAN();
  logTWAIStatus();
  if (!isTWAIStarted()) {
    logMsg(LOG_WARN, "TWAI", "No está RUNNING. Revisa twai_start() / wiring / bitrate");
  }

  // Esperar SoC por CAN (máx 3 s) ANTES de tocar el módem
  soc = 0xFF; // SOC_UNKNOWN
  logMsg(LOG_INFO, "SOC", "Esperando SoC por CAN (máx 3 s)...");
  startWindow("WAIT_SOC_BOOT");
  unsigned long startWait = millis();
  while (millis() - startWait < 3000 && soc == 0xFF) {
    checkCANInput();   // intenta recibir 0x541
    delay(10);
  }
  endWindow("WAIT_SOC_BOOT");
  if (soc != 0xFF) {
    logMsg(LOG_INFO, "SOC", String("SoC al arrancar: ") + String(soc) + "%");
  } else {
    logMsg(LOG_WARN, "SOC", "No se recibió SoC. Continuando sin él. (Set a 0 para pruebas)");
    soc = 0;  // neutro para pruebas
  }

  // === Módem ===
logMark("MODEM_INIT");
initModem();
logMsg(LOG_INFO, "MODEM", "Inicializado");

// Mutex del módem
modemMutex = xSemaphoreCreateMutex();
if (!modemMutex) logMsg(LOG_ERROR, "MUTEX", "Error creando mutex del módem");
else             logMsg(LOG_INFO,  "MUTEX", "Creado mutex del módem");

// ——— Intento rápido inicial (no bloqueante)
logMsg(LOG_INFO, "GPRS", "Conectando (intento rápido)...");
gprsConnected = ensureDataAttached();
if (gprsConnected) {
  logMsg(LOG_INFO, "GPRS", "Conectado");
  vTaskDelay(pdMS_TO_TICKS(1500));
} else {
  logMsg(LOG_WARN, "GPRS", "No conectado (NetWatchdog seguirá intentando)");
}

// Hora desde módem (si hay red) — protegido por mutex
if (gprsConnected && modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
  startWindow("GET_TIME_FROM_MODEM");
  int h=0, m=0;
  if (getTimeFromModem(h, m)) {
    localHour = h; minute = m; hora_valida = true;
    logMsg(LOG_INFO, "TIME", String("Hora desde módem: ") + h + ":" + (m<10?"0":"") + m);
  } else {
    logMsg(LOG_WARN, "TIME", "No se pudo obtener la hora del módem");
  }
  endWindow("GET_TIME_FROM_MODEM");
  xSemaphoreGive(modemMutex);
}

// === Tareas === (SIEMPRE) — Core 0: CAN
xTaskCreatePinnedToCore(taskCANProcessing, "CANTask",     4096, NULL, 2, NULL, 0);
logMsg(LOG_INFO, "TASK", "CANTask creada en core 0");
xTaskCreatePinnedToCore(taskSendCANHour,  "CANHourTask",  2048, NULL, 1, NULL, 0);
logMsg(LOG_INFO, "TASK", "CANHourTask creada en core 0");

// Core 1: Telecom (módem)
xTaskCreatePinnedToCore(taskNetWatchdog,  "NetWD",        4096, NULL, 2, NULL, 1);   // <—— NUEVA
logMsg(LOG_INFO, "TASK", "NetWatchdog creada en core 1");
xTaskCreatePinnedToCore(taskTelegram,     "TelegramTask", 8192, NULL, 2, NULL, 1);
logMsg(LOG_INFO, "TASK", "TelegramTask creada en core 1");
xTaskCreatePinnedToCore(taskGPS,          "GPSTask",      4096, NULL, 1, NULL, 1);
logMsg(LOG_INFO, "TASK", "GPSTask creada en core 1");

// Periféricos
initSD();
logMsg(LOG_INFO, "SD", "initSD llamado");

logMark("SETUP_END");
}

// ========= loop (no bloqueante, sin tocar módem) =========
void loop() {
  static TickType_t lastWake = xTaskGetTickCount();
  static uint32_t lastHbMs = 0;

  checkSoC();               // alerta y apagado si batería baja (no bloqueante)
  handleModoDiagnostico();  // lógica de modo diagnóstico (no debe tocar módem)

  // Heartbeat cada 5 s con estado resumido
  uint32_t now = millis();
  if (now - lastHbMs > 5000) {
    lastHbMs = now;
    String st = String("soc=") + String(soc) +
                " gprs=" + (gprsConnected ? "1" : "0") +
                (hora_valida ? (" time=" + String(localHour) + ":" + (minute<10?"0":"") + String(minute)) : " time=NA");
    heartbeat(st);
  }

  // GPS se gestiona en taskGPS con mutex del módem
  vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(100)); // 10 Hz estable
}

void taskTelegram(void *pvParameters) {
  bool inited = false;
  TickType_t lastWake = xTaskGetTickCount();

  // Pequeño respiro al sistema antes de empezar
  vTaskDelay(pdMS_TO_TICKS(250));
  logMark("TELEGRAM_TASK_START");

  for (;;) {
    // Si no hay GPRS o aún no existe el mutex → no intentamos nada
    if (!gprsConnected || modemMutex == nullptr) {
      if (inited) logMsg(LOG_WARN, "TELEGRAM", "GPRS OFF o sin mutex → reinicio suave");
      inited = false;
      vTaskDelay(pdMS_TO_TICKS(1000));
      // re-sincronizamos la temporización de vTaskDelayUntil
      lastWake = xTaskGetTickCount();
      continue;
    }

    // Primer arranque / reintento tras pérdida de GPRS
    if (!inited) {
      // warm-up corto para que el resto de tareas estabilicen
      vTaskDelay(pdMS_TO_TICKS(1000));
      logMsg(LOG_INFO, "TELEGRAM", "Init…");

      // 1) Handshake TLS (no envía bienvenida)
      bool ok_init = initTelegram();

      if (ok_init) {
        // 2) Bienvenida idempotente (solo una vez)
        for (int i = 0; i < 3 && !sendWelcomeOnce(); ++i) {
          vTaskDelay(pdMS_TO_TICKS(700));
        }

        // 3) Burst de polling 5s para absorber backlog
        uint32_t t0 = millis();
        while (millis() - t0 < 5000) {
          (void)checkTelegram(millis());
          vTaskDelay(pdMS_TO_TICKS(300));
        }
      }

      inited = ok_init;  // si falló, volverá a intentarlo en el siguiente ciclo
      // re-sincronizamos el reloj del vTaskDelayUntil tras los delays anteriores
      lastWake = xTaskGetTickCount();
      continue;
    }

    startWindow("TELEGRAM_POLL");
    (void)checkTelegram(millis());
    endWindow("TELEGRAM_POLL");

    // si el poll tardó más de ~500 ms, resetea el punto de referencia
    static const TickType_t CATCHUP_MS = 500;
    if (xTaskGetTickCount() - lastWake > pdMS_TO_TICKS(CATCHUP_MS)) {
      lastWake = xTaskGetTickCount();
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));
  }
}








// ========= Tarea: GPS (core 1) =========
// Encargada de activar/leer/apagar GPS bajo demanda.
// checkGPSStatus() debe decidir internamente si debe actuar y puede usar el logger.
void taskGPS(void *pvParameters) {
  logMark("GPS_TASK_START");
  for (;;) {
    if (gprsConnected && modemMutex) {
      if (xSemaphoreTake(modemMutex, pdMS_TO_TICKS(7000)) == pdTRUE) {
        // Dentro de checkGPSStatus() puedes abrir ventanas más finas:
        //   startWindow("GPS_FIX_SEARCH"); ... endWindow("GPS_FIX_SEARCH");
        //   logMark("GPS_ON"), logMark("GPS_OFF"), etc.
        logMsg(LOG_DEBUG, "GPS", "tick");
        checkGPSStatus(millis());  // gestionará ON→fix→OFF si procede
        xSemaphoreGive(modemMutex);
      } else {
        logMsg(LOG_WARN, "MUTEX", "GPS no pudo tomar el mutex (timeout)");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Hz
  }
}
