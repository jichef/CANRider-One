#include <driver/twai.h>

// Definiciones importantes para TinyGSM y el proyecto
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include "globals.h"
#include "output.h"
#include "modem.h"
#include "sms.h"
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

BluetoothSerial SerialBT;
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
HttpClient http(client, server);

// Tamaño de la pila para las tareas (en palabras, no en bytes)
#define STACK_SIZE 4096 // Tamaño de pila recomendado para tareas FreeRTOS

#define GPS_TASK_PRIORITY 2 // Mayor prioridad para GPS
#define CAN_TASK_PRIORITY 1 // Menor prioridad para CAN

QueueHandle_t canQueue;
const int rx_queue_size = 50;  // Aumenta el tamaño de la cola

// Configuración global
twai_general_config_t g_config;
twai_timing_config_t t_config;
twai_filter_config_t f_config;

float lat = 0.0;  
float lon = 0.0;  

void sendToTraccar(float lat, float lon, float speed, float alt, float accuracy, float battery, int batterylevel);

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Inicializar salidas
    initializeOutput(SerialBT, tft);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    // Encender el módem y conectarse a la red
    modemPowerOn();
    initializeModem(modem);

    if (!connectToNetwork(modem, apn, gprsUser, gprsPass)) {
        logToOutputln(GPRS_CONNECTION_FAILED);
        modemRestart();
    }

    logToOutputln(INIT_TWAI);
    g_config = {
        .mode = TWAI_MODE_NORMAL,   // Cambiar a modo normal para enviar y recibir mensajes
        .tx_io = GPIO_NUM_32,   // Pin TX cambiado a 32
        .rx_io = GPIO_NUM_33,   // Pin RX cambiado a 33
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 10,
        .rx_queue_len = 10,
        .alerts_enabled = 0,  // No se necesitan alertas
        .clkout_divider = 0,  // Sin salida de reloj
        .intr_flags = 0,      // Sin interrupciones
    };

    t_config = TWAI_TIMING_CONFIG_250KBITS();  // Configuración inicial de 250 kbps
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Acepta todos los mensajes

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        logToOutputln(INIT_TWAI_INSTALL_OK);
        driver_installed = true;
    } else {
        logToOutputln(INIT_TWAI_INSTALL_ERROR);
        driver_installed = false;
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
        driver_installed = false;
    }

    enableGPS(modem);

    logToOutputln(SYSTEM_INITIALIZED);
    
    xTaskCreate(taskGPSTraccar, "TaskGPSTraccar", 4096, NULL, 1, NULL); // Aumenta el tamaño de la pila si es necesario
    xTaskCreate(taskCANProcessing, "CANTask", STACK_SIZE, NULL, CAN_TASK_PRIORITY, NULL);
}

void loop() {

}

void taskGPSTraccar(void *pvParameters) {
    float lat, lon, speed, alt, accuracy;

    for (;;) { // Bucle infinito de la tarea
        // Leer la batería ESP
        battery = ReadBattery();
        
        // Leer datos de GPS
        if (!getGPSData(modem, lat, lon, speed, alt, accuracy)) {
            logToOutputln(GPS_DATA_RETRY);
        } else {
            // Enviar datos a Traccar
            sendToTraccar(lat, lon, speed, alt, accuracy, battery, batterylevel);
        } 
        // Determinar frecuencia de envío según el estado del vehículo
        if (battery == 0) {
            // Si la batería está en 0, comprobamos SMS más rápido
            checkForSMS(modem);  // Verifica si hay SMS nuevos
            logToOutputln(VEHI_ON + String(VEHIEncendidoDelay / 1000) + " segundos.");
            vTaskDelay(pdMS_TO_TICKS(VEHIEncendidoDelay)); // Vehículo encendido, comprobamos más rápido
        } else {
            // Si la batería no está en 0, comprobamos SMS con más tiempo entre verificaciones
            checkForSMS(modem);  // Verifica si hay SMS nuevos
            logToOutput(BATTERY_VOLTAGE_ESP);
            logToOutputln(String(battery, 2) + "V");
            logToOutputln(VEHI_OFF + String(VEHIApagadoDelay / 1000) + " segundos.");
            vTaskDelay(pdMS_TO_TICKS(VEHIApagadoDelay)); // Vehículo apagado, comprobamos más lentamente
        }

      

        // Asegurarse de que haya suficiente tiempo para procesar los SMS
        vTaskDelay(pdMS_TO_TICKS(100));  // Agregar un pequeño retraso entre ciclos
    }
}


unsigned long previousMillis = 0;  // Variable para controlar el tiempo
float lastBatteryState = -1; // Variable para almacenar el último estado de la batería

void taskCANProcessing(void *pvParameters) {
    for (;;) {
        twai_message_t rx_message;

        // Intentar recibir un mensaje CAN con un tiempo de espera de 3 ms
        if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
            // Verifica si el ID del mensaje es el esperado (ID 0x541)
            if (rx_message.identifier == 0x541) {
                // Guardar el primer byte del mensaje como batterylevel
                batterylevel = rx_message.data[0];

                // Solo imprimir si el valor de batterylevel ha cambiado
                if (batterylevel != lastBatteryState) {
                    lastBatteryState = batterylevel;
                    displayBatteryState();
                }

                // Intentar enviar el mensaje recibido a la cola con tiempo de espera
                if (xQueueSend(canQueue, &rx_message, pdMS_TO_TICKS(10)) == pdTRUE) {
                } else {
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(3));  // 3ms de retraso
    }
}

void sendToTraccar(float lat, float lon, float speed, float alt, float accuracy, float battery, int batterylevel) {
    logToOutputln(SENDING_TO_TRACCAR);

    // Calcular nivel de batería como porcentaje
    float batteryState = (((float)battery - 3.0) / 1.2) * 100.0;
    if (batteryState > 100) batteryState = 100;
    if (batteryState < 0) batteryState = 0;

    // Determinar el estado de encendido
    const char *ignition = (battery == 0) ? "true" : "false";

    // Construir la URL de la solicitud
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
