#include <Arduino.h>
#include "driver/twai.h"

#include "can_bus.h"
#include "modem.h"
#include "logs.h"
#include "utils.h"

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
}

// ================== Externs de otros módulos ==================
extern int  localHour;         // espejos legacy; no escribir fuera de este archivo
extern int  minute;
extern int  soc;
extern bool hora_valida;
extern SemaphoreHandle_t modemMutex;

// ================== Firma emisor (diagnóstico) ==================
#define CAN_TX_SIGNATURE 0xA1   // firma en data[0] para detectar duplicados

// ================== Sonda opcional por GPIO ==================
// Pulso en cada intento de envío (útil para analizador lógico)
#define CAN_TX_PROBE_PIN GPIO_NUM_4  // <- cambia pin o comenta esta línea para desactivar

// ================== Estado de tareas (idempotencia) ==================
static volatile bool g_hour_tasks_started = false;

// ================== Empaquetado de hora:min (sin mutex) ==================
//   g_time_hm: [15:8] = hour (0..23), [7:0] = minute (0..59)
static volatile uint16_t g_time_hm    = 0;
static volatile bool     g_time_ready = false;

// (diagnóstico) contador de secuencia y métricas de envío
static volatile uint8_t  g_tx_seq     = 0;
static volatile uint32_t g_tx_ok      = 0;
static volatile uint32_t g_tx_fail    = 0;

// ---------- helpers internos ----------
static inline void time_init_from_globals() {
  uint8_t h = (localHour >= 0 && localHour <= 23) ? (uint8_t)localHour : 0;
  uint8_t m = (minute    >= 0 && minute    <= 59) ? (uint8_t)minute    : 0;
  g_time_hm = (uint16_t(h) << 8) | uint16_t(m);
  g_time_ready = (h != 0 || m != 0);
}

static inline void time_set(uint8_t h, uint8_t m) {
  g_time_hm = (uint16_t(h) << 8) | uint16_t(m);
  // espejos legacy (para logs/heartbeat externos)
  localHour = h;
  minute    = m;
  g_time_ready = true;
}

static inline void time_get(uint8_t &h, uint8_t &m) {
  uint16_t snap = g_time_hm;   // lectura atómica (16-bit alineado en ESP32)
  h = uint8_t(snap >> 8);
  m = uint8_t(snap & 0xFF);
}

// ---------- wrappers públicos de hora ----------
bool time_ready() { return g_time_ready; }
void time_get_hm(uint8_t& h, uint8_t& m) { time_get(h, m); }
void time_set_hm(uint8_t h, uint8_t m)   { time_set(h, m); }
void time_seed_from_setup()              { time_init_from_globals(); }

// ================== Helpers TWAI ==================
static const char* twaiStateName(twai_state_t st) {
  switch (st) {
    case TWAI_STATE_STOPPED:    return "STOPPED";
    case TWAI_STATE_RUNNING:    return "RUNNING";
    case TWAI_STATE_BUS_OFF:    return "BUS_OFF";
    case TWAI_STATE_RECOVERING: return "RECOVERING";
    default:                    return "UNKNOWN";
  }
}

bool isTWAIStarted() {
  twai_status_info_t s;
  if (twai_get_status_info(&s) != ESP_OK) return false;
  return s.state == TWAI_STATE_RUNNING;
}

void logTWAIStatus(const char* tag) {
  twai_status_info_t s;
  if (twai_get_status_info(&s) != ESP_OK) {
    logMsg(LOG_ERROR, tag, "No instalado (twai_get_status_info falló)");
    return;
  }
  String msg = String("state=") + twaiStateName(s.state) +
               " rxq=" + String(s.msgs_to_rx) +
               " txq=" + String(s.msgs_to_tx) +
               " tx_err=" + String(s.tx_error_counter) +
               " rx_err=" + String(s.rx_error_counter) +
               " bus_err=" + String(s.bus_error_count) +
               " arb_lost=" + String(s.arb_lost_count);
  logMsg(LOG_INFO, tag, msg);
}

// ================== Inicialización CAN (TWAI) ==================
void initCAN() {
  // Pines: TX=33, RX=32 (ajusta si difieren)
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_33, GPIO_NUM_32, TWAI_MODE_NORMAL);
  g.tx_queue_len   = 20;
  g.rx_queue_len   = 20;
  g.alerts_enabled = 0;   // habilita alertas si quieres diagnóstico fino

  // Velocidad típica CPX: 250 kbps
  twai_timing_config_t  t = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t e = twai_driver_install(&g, &t, &f);
  if (e != ESP_OK) { logMsg(LOG_ERROR, "TWAI", String("driver_install: ") + (int)e); return; }

  e = twai_start();
  if (e != ESP_OK) { logMsg(LOG_ERROR, "TWAI", String("start: ") + (int)e); return; }

#ifdef CAN_TX_PROBE_PIN
  pinMode(CAN_TX_PROBE_PIN, OUTPUT);
  digitalWrite(CAN_TX_PROBE_PIN, LOW);
#endif

  logMsg(LOG_INFO, "TWAI", "Instalado y RUNNING");
}

// ================== Lectura rápida en setup() (esperar SoC) ==================
void checkCANInput() {
  twai_message_t rx_message;
  if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
    if (rx_message.identifier == CAN_ID_SOC && rx_message.data_length_code >= 1) {
      soc = rx_message.data[0];
    }
  }
}

// ================== Envío por CAN ==================
// Versión de compatibilidad (no usar en la tarea periódica)
void sendHourViaCAN(int hour, int minute) {
  twai_message_t message = {};
  message.identifier = CAN_ID_HORA;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 8;

  message.data[0] = CAN_TX_SIGNATURE;
  message.data[1] = 0x00;                 // sin secuencia aquí
  message.data[2] = 0x01;
  message.data[3] = 0x00;
  message.data[4] = 0x70;
  message.data[5] = (uint8_t)hour;
  message.data[6] = (uint8_t)minute;
  message.data[7] = 0x00;

#ifdef CAN_TX_PROBE_PIN
  digitalWrite(CAN_TX_PROBE_PIN, !digitalRead(CAN_TX_PROBE_PIN)); // pulso
#endif

  if (twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    g_tx_ok++;
  } else {
    g_tx_fail++;
  }
}

// “Blindada”: lee snapshot interno y no envía si no hay hora lista ni 00:00
void sendHourViaCAN_now() {
  if (!g_time_ready) return;

  uint8_t h, m; 
  time_get(h, m);
  if (h == 0 && m == 0) return;   // guardia anti 00:00

  twai_message_t message = {};
  message.identifier = CAN_ID_HORA;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 8;

  message.data[0] = CAN_TX_SIGNATURE;
  message.data[1] = g_tx_seq;     // secuencia (útil para ver ritmo en receptor)
  message.data[2] = 0x01;
  message.data[3] = 0x00;
  message.data[4] = 0x70;
  message.data[5] = h;
  message.data[6] = m;
  message.data[7] = 0x00;

#ifdef CAN_TX_PROBE_PIN
  digitalWrite(CAN_TX_PROBE_PIN, !digitalRead(CAN_TX_PROBE_PIN)); // pulso
#endif

  if (twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    g_tx_seq++;
    g_tx_ok++;
  } else {
    g_tx_fail++;
  }
}

// ================== Tareas ==================

// Tarea 1 — Mantener hora: +1 min cada 60 s, resync módem cada 60 min
static void taskClockTicker(void *pvParameters) {
  // Semilla inicial desde globals (por si no llamaron a time_seed_from_setup)
  time_init_from_globals();

  TickType_t lastWake       = xTaskGetTickCount();
  TickType_t lastMinuteTick = lastWake;
  TickType_t lastModemTick  = lastWake - pdMS_TO_TICKS(3600000UL); // fuerza 1er resync

  const TickType_t kPeriod = pdMS_TO_TICKS(50); // latido ligero

  for (;;) {
    vTaskDelayUntil(&lastWake, kPeriod);
    TickType_t now = xTaskGetTickCount();

    // Tick de minuto
    if ((now - lastMinuteTick) >= pdMS_TO_TICKS(60000)) {
      uint8_t h, m; time_get(h, m);
      m++;
      if (m >= 60) { m = 0; h = (h + 1) % 24; }
      time_set(h, m);
      lastMinuteTick = now;
    }

    // Resync con módem cada 60 min (bloqueo permitido aquí)
    if ((now - lastModemTick) >= pdMS_TO_TICKS(3600000UL)) {
      if (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(7000)) == pdTRUE) {
        int hh = 0, mm = 0;
        bool ok = getTimeFromModem(hh, mm);
        xSemaphoreGive(modemMutex);

        if (ok && (hh>=0 && hh<=23) && (mm>=0 && mm<=59) && !(hh==0 && mm==0)) {
          time_set((uint8_t)hh, (uint8_t)mm);
          hora_valida = true;
          Serial.printf("🕒 Hora resync desde módem: %02d:%02d\n", hh, mm);
        } else {
          Serial.println("⚠️ Resync hora inválido; conservo la anterior");
        }
      }
      lastModemTick = now;
    }
  }
}

// Tarea 2 — Enviar hora por CAN cada 200 ms (instrumentada)
static void taskSendCANHour(void *pvParameters) {
  const TickType_t kPeriodCAN = pdMS_TO_TICKS(200);
  TickType_t lastWake = xTaskGetTickCount();

  // Métricas de periodo (por ventana de ≈1s)
  TickType_t prev = lastWake;
  uint32_t   cnt_period = 0;
  uint32_t   min_ms = 1000000, max_ms = 0, sum_ms = 0;

  // Métricas de envíos
  uint32_t   last_report_ms = millis();
  uint32_t   last_ok = 0, last_fail = 0;

  for (;;) {
    vTaskDelayUntil(&lastWake, kPeriodCAN);

    // Periodo real (en ms) entre iteraciones
    TickType_t nowTicks = xTaskGetTickCount();
    uint32_t   dt_ms = (nowTicks - prev) * portTICK_PERIOD_MS;
    prev = nowTicks;

    if (dt_ms < min_ms) min_ms = dt_ms;
    if (dt_ms > max_ms) max_ms = dt_ms;
    sum_ms += dt_ms;
    cnt_period++;

    // Envío (snapshot interno con guardias)
    sendHourViaCAN_now();

    // Reporte cada ~1s
    uint32_t now = millis();
    if (now - last_report_ms >= 1000) {
      uint32_t ok   = g_tx_ok;
      uint32_t fail = g_tx_fail;
      uint32_t ok_delta   = ok   - last_ok;
      uint32_t fail_delta = fail - last_fail;

      uint32_t avg_ms = cnt_period ? (sum_ms / cnt_period) : 0;
//      logMsg(LOG_DEBUG, "CAN_TX",
//        String("loop(ms) min/avg/max=") + min_ms + "/" + avg_ms + "/" + max_ms +
 //       " | sent=" + ok_delta + " fail=" + fail_delta);

      // reset ventana de 1s
      min_ms = 1000000; max_ms = 0; sum_ms = 0; cnt_period = 0;
      last_ok = ok; last_fail = fail; last_report_ms = now;
    }
  }
}

// RX continuo de CAN (SoC 0x541)
void taskCANProcessing(void *pvParameters) {
  for (;;) {
    twai_message_t rx_message;
    if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
      if (rx_message.identifier == CAN_ID_SOC && rx_message.data_length_code >= 1) {
        soc = rx_message.data[0];
      }
    }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

// Arranque de tareas (idempotente)
void startHourTasks() {
  if (g_hour_tasks_started) {
    logMsg(LOG_WARN, "TIME_TASKS", "startHourTasks() llamado de nuevo — ignorado");
    return;
  }
  g_hour_tasks_started = true;

  // Mismo core que TWAI (core 0) para latencia consistente; TX un pelín más prioritario
  xTaskCreatePinnedToCore(taskClockTicker,  "CLOCK_TICK",  4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(taskSendCANHour,  "CAN_HOUR_TX", 4096, nullptr, 3, nullptr, 0);

//  logMsg(LOG_INFO, "TIME_TASKS", "CLOCK_TICK + CAN_HOUR_TX lanzadas");
}
