#include <Arduino.h>
#include "driver/twai.h"

#include "utils.h"
#include "can_bus.h"
#include "modem.h"
#include "logs.h"                 // <- macros LOG_* y logMsg

// FreeRTOS (para usar el mutex del módem en esta tarea)
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
}

// ================== IDs CAN ==================
#define CAN_ID_HORA 0x510
#define CAN_ID_SOC  0x541

// ================== EXTERNS ==================
extern int localHour;
extern int minute;
extern int soc;
extern bool hora_valida;

// Usamos el mismo mutex del módem para proteger lecturas de hora
extern SemaphoreHandle_t modemMutex;

// (si usas una cola CAN en otro sitio)
extern QueueHandle_t canQueue;   // opcional; no se usa aquí

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
  if (twai_get_status_info(&s) != ESP_OK) return false;  // driver no instalado
  return s.state == TWAI_STATE_RUNNING;
}

void logTWAIStatus(const char* tag /*= "TWAI"*/) {
  twai_status_info_t s;
  esp_err_t err = twai_get_status_info(&s);
  if (err != ESP_OK) {
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
  // Pines: ajusta a los tuyos reales si difieren
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_33, GPIO_NUM_32, TWAI_MODE_NORMAL);
  g.tx_queue_len = 10;
  g.rx_queue_len = 10;
  g.alerts_enabled = 0;  // sin alertas por ahora

  // VELOCIDAD: AJUSTA según tu moto/bus (250k habitual en scooters, 500k en automoción)
  twai_timing_config_t  t = TWAI_TIMING_CONFIG_250KBITS();
  // twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();

  twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t e = twai_driver_install(&g, &t, &f);
  if (e != ESP_OK) {
    logMsg(LOG_ERROR, "TWAI", String("driver_install: ") + (int)e);
    return;
  }
  e = twai_start();
  if (e != ESP_OK) {
    logMsg(LOG_ERROR, "TWAI", String("start: ") + (int)e);
    return;
  }
  logMsg(LOG_INFO, "TWAI", "Instalado y RUNNING");
}

// ================== Lectura rápida en setup() ==================
void checkCANInput() {
  twai_message_t rx_message;
  if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
    if (rx_message.identifier == CAN_ID_SOC && rx_message.data_length_code >= 1) {
      soc = rx_message.data[0];
      // log de depuración (opcional)
      logToOutput("🔋 SoC recibido: ");
      logToOutputln(String(soc) + "%");
    }
  }
}

// ================== Envío de hora por CAN ==================
void sendHourViaCAN(int hour, int minute) {
  twai_message_t message = {};
  message.identifier = CAN_ID_HORA;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 8;

  message.data[0] = 0x00;
  message.data[1] = 0x00;
  message.data[2] = 0x01;
  message.data[3] = 0x00;
  message.data[4] = 0x70;
  message.data[5] = (uint8_t)hour;
  message.data[6] = (uint8_t)minute;
  message.data[7] = 0x00;

  if (twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    // logs compactos para no saturar serie
    // logToOutput("→ CAN hora enviada: ");
    // if (hour < 10) logToOutput("0");
    // logToOutput(String(hour)); logToOutput(":");
    // if (minute < 10) logToOutput("0");
    // logToOutputln(String(minute));
  } else {
    // logToOutputln("× Error al enviar hora por CAN");
  }
}

// ================== Tareas ==================
void taskSendCANHour(void *pvParameters) {
  static unsigned long lastMinuteUpdate = 0;
  static unsigned long lastModemSync = 0;

  for (;;) {
    unsigned long now = millis();

    // ⏱️ Ticker: +1 minuto cada 60 s
    if (now - lastMinuteUpdate >= 60000 || lastMinuteUpdate == 0) {
      minute++;
      if (minute >= 60) {
        minute = 0;
        localHour = (localHour + 1) % 24;
      }
      lastMinuteUpdate = now;
    }

    // 🔄 Resync con módem cada 60 min (validación básica y protegido por mutex)
    if (now - lastModemSync >= 3600000UL || lastModemSync == 0) {
      if (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(7000)) == pdTRUE) {
        int h = 0, m = 0;
        bool ok = getTimeFromModem(h, m);
        xSemaphoreGive(modemMutex);

        if (ok && (h >= 0 && h <= 23) && (m >= 0 && m <= 59) && !(h == 0 && m == 0)) {
          localHour = h;
          minute    = m;
          Serial.printf("🕒 Hora resync desde módem: %02d:%02d\n", localHour, minute);
        } else {
          Serial.println("⚠️ Resync hora inválido; conservo la anterior");
        }
      }
      lastModemSync = now;
    }

    // 📤 Enviar la hora actual por CAN
    sendHourViaCAN(localHour, minute);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void taskCANProcessing(void *pvParameters) {
  for (;;) {
    twai_message_t rx_message;
    if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
      if (rx_message.identifier == CAN_ID_SOC && rx_message.data_length_code >= 1) {
        soc = rx_message.data[0];
        // logToOutput("🔋 SoC recibido: "); logToOutputln(String(soc) + "%");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}
