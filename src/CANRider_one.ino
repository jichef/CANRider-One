#include <driver/twai.h>
#include "globals.h"
#include "output.h"
#include "modem.h"
#include "sms.h"
#include "timeutils.h"
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <BluetoothSerial.h>



// Definiciones para TinyGSM y configuración
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024

BluetoothSerial SerialBT;


TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
HttpClient http(client, server);

// Tamaño de pila para tareas
#define STACK_SIZE 4096
#define GPS_TASK_PRIORITY 2
#define CAN_TASK_PRIORITY 1

QueueHandle_t canQueue;
const int rx_queue_size = 50;

twai_general_config_t g_config;
twai_timing_config_t t_config;
twai_filter_config_t f_config;

float lat = 0.0;
float lon = 0.0;


TaskHandle_t canTaskHandle = NULL;  // Handle de la tarea CAN

void sendToTraccar(float lat, float lon, float speed, float alt, float accuracy, float battery, int batterylevel);

void setup() {
    Serial.begin(115200);
    delay(2000);

    initializeOutput(SerialBT);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    modemPowerOn();
    initializeModem(modem);
    
    xTaskCreatePinnedToCore(taskSMSChecker, "SMSTask", 4096, NULL, 1, NULL, 1);
    if (!connectToNetwork(modem, apn, gprsUser, gprsPass)) {
        logToOutputln(GPRS_CONNECTION_FAILED);
        modemRestart();
    }

    logToOutputln(INIT_TWAI);
    g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_NUM_32,
        .rx_io = GPIO_NUM_33,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 10,
        .rx_queue_len = 10,
        .alerts_enabled = 0,
        .clkout_divider = 0,
        .intr_flags = 0,
    };

    t_config = TWAI_TIMING_CONFIG_250KBITS();
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        logToOutputln(INIT_TWAI_INSTALL_OK);
    } else {
        logToOutputln(INIT_TWAI_INSTALL_ERROR);
        return;
    }

    canQueue = xQueueCreate(rx_queue_size, sizeof(twai_message_t));
    if (canQueue == NULL) {
        logToOutputln("Error al crear la cola");
    }

    if (twai_start() == ESP_OK) {
        logToOutputln(INIT_TWAI_SUCCESS);
    } else {
        logToOutputln(INIT_TWAI_ERROR);
    }
    if (!connectToNetwork(modem, apn, gprsUser, gprsPass)) {
    logToOutputln(GPRS_CONNECTION_FAILED);
    modemRestart();
} else {
    delay(1000);  // 🕐 Pequeña espera para que se estabilice la red
   syncTimeFromHTTPHeader(modem);

}

    enableGPS(modem);
    logToOutputln(SYSTEM_INITIALIZED);
    
    xTaskCreatePinnedToCore(taskInternalClock, "ClockTask", 2048, NULL, 1, NULL, 1);
    xTaskCreate(taskGPSTraccar, "TaskGPSTraccar", STACK_SIZE, NULL, GPS_TASK_PRIORITY, NULL);
    xTaskCreate(taskCANProcessing, "CANTask", STACK_SIZE, NULL, CAN_TASK_PRIORITY, &canTaskHandle);
    xTaskCreatePinnedToCore(taskSendCANHour, "CANHourTask", 2048, NULL, 1, NULL, 1);

}

void loop() {
    // No es necesario usar loop, todo se maneja en tareas
}

void taskInternalClock(void *pvParameters) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(60000));  // Espera 60 segundos

    minute++;
    if (minute >= 60) {
      minute = 0;
      localHour++;
      if (localHour >= 24) {
        localHour = 0;
      }
    }
  }
}



void taskSMSChecker(void *pvParameters) {
    for (;;) {
        checkForSMS(modem);
        Serial.println("⏳ Revisando SMS...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskSendCANHour(void *pvParameters) {
  for (;;) {
    // Aquí envías `localHour` por CAN (y si quieres, también `min`)
    sendHourViaCAN(localHour, minute);

    vTaskDelay(pdMS_TO_TICKS(200)); // Espera 200ms
  }
}

void sendHourViaCAN(int hour, int minute) {
  twai_message_t message = {
    message.identifier = 0x510,
    message.extd = 0,
    message.rtr = 0,
    message.data_length_code = 8
  };

  message.data[0] = 0x00;
  message.data[1] = 0x00;
  message.data[2] = 0x01;
  message.data[3] = 0x00;
  message.data[4] = 0x70;
  message.data[5] = (uint8_t)hour;
  message.data[6] = (uint8_t)minute;
  message.data[7] = 0x00;

  if (twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    logToOutput("→ CAN hora enviada: ");
    if (hour < 10) logToOutput("0");
    logToOutput(String(hour));
    logToOutput(":");
    if (minute < 10) logToOutput("0");
    logToOutputln(String(minute));
  } else {
    logToOutputln("× Error al enviar hora por CAN");
  }
}

void taskGPSTraccar(void *pvParameters) {
    float lat, lon, speed, alt, accuracy;

    for (;;) {
        battery = ReadBattery();  // Leer la batería del ESP

        // Control de la tarea CAN según el estado de la batería
        if (battery != 0 && canTaskHandle != NULL) {
            vTaskSuspend(canTaskHandle);  // Suspender tarea CAN si la moto está apagada
        } else if (battery == 0 && canTaskHandle != NULL) {
            vTaskResume(canTaskHandle);   // Reanudar tarea CAN si la moto está encendida
        }

        // Leer datos de GPS
        if (!getGPSData(modem, lat, lon, speed, alt, accuracy)) {
            logToOutputln(GPS_DATA_RETRY);
        } else {
            sendToTraccar(lat, lon, speed, alt, accuracy, battery, batterylevel);
        }

        // Ajustar frecuencia de verificación según el estado del vehículo
        
        if (battery == 0) {
            logToOutputln(VEHI_ON + String(VEHIEncendidoDelay / 1000) + " segundos.");
            vTaskDelay(pdMS_TO_TICKS(VEHIEncendidoDelay)); // Moto encendida, chequeo rápido
        } else {
            logToOutputln(VEHI_OFF + String(VEHIApagadoDelay / 1000) + " segundos.");
            vTaskDelay(pdMS_TO_TICKS(VEHIApagadoDelay)); // Moto apagada, chequeo lento
        }
    }
}

void taskCANProcessing(void *pvParameters) {
    for (;;) {
        twai_message_t rx_message;

        if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {

            if (rx_message.identifier == 0x541) {  // Trama CAN para monitorear: 540
                batterylevel = rx_message.data[0]; // Trama CAN para monitorear: primer byte
                

                if (xQueueSend(canQueue, &rx_message, pdMS_TO_TICKS(10)) != pdTRUE) {
                    logToOutputln("Error: Cola CAN llena");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(3));
    }
}

void sendToTraccar(float lat, float lon, float speed, float alt, float accuracy, float battery, int batterylevel) {
    logToOutputln(SENDING_TO_TRACCAR);

    float batteryState = (((float)battery - 3.0) / 1.2) * 100.0;
    batteryState = constrain(batteryState, 0, 100);

    const char *ignition = (battery == 0) ? "true" : "false";

    char url[256];
    snprintf(url, sizeof(url),
             "/?id=%s&lat=%.6f&lon=%.6f&accuracy=%.2f&altitude=%.2f&speed=%.2f&batteryLevel=%d&ignition=%s&batteryState=%.0f&battery=%.2f",
             myid.c_str(), lat, lon, accuracy, alt, speed, batterylevel, ignition, batteryState, battery);

    logToOutputln(TRACCAR_URL);
    logToOutputln(String(url));

    int err = http.post(url);
    if (err != 0) {
        logToOutputln(TRACCAR_CONNECTION_ERROR);
        return;
    }

    int statusCode = http.responseStatusCode();
    if (statusCode == 200) {
        logToOutputln(TRACCAR_SUCCESS);
    } else {
        logToOutput(TRACCAR_HTTP_ERROR);
        logToOutputln(String(statusCode));
    }

    http.stop();
    logToOutputln(TRACCAR_CONNECTION_CLOSED);
}
