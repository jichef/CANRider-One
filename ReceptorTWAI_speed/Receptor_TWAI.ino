#define LILYGO_T_A7670
#include "utilities.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "driver/twai.h"
#include <BluetoothSerial.h> // Biblioteca para Bluetooth Serial

// Pines TWAI
#define TX_PIN 32
#define RX_PIN 33

// Pines SPI y SD
#define SD_CS_PIN 13

// LED integrado
#define LED_PIN 12

// Bluetooth Serial
BluetoothSerial SerialBT;

// Archivo para registro
String logFilePath = "";
String messageBuffer = "";

// Función para determinar el nombre del archivo numerado
String generateFileName() {
    int fileNumber = 1;
    String baseName = "/twai_log_";
    String extension = ".txt";

    while (true) {
        String path = baseName + String(fileNumber) + extension;
        if (!SD.exists(path.c_str())) {
            return path; // Archivo no existe, usar este nombre
        }
        fileNumber++;
    }
}

void initSDCard() {
    Serial.println("Inicializando tarjeta SD...");
    SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, 1000000)) {
        Serial.println("Error al montar la tarjeta SD.");
        return;
    }

    Serial.println("Tarjeta SD montada correctamente.");
    logFilePath = generateFileName();
    File file = SD.open(logFilePath, FILE_WRITE);
    if (file) {
        file.println("Inicio del registro de mensajes TWAI a 250 kbps");
        file.close();
        Serial.printf("Archivo creado: %s\n", logFilePath.c_str());
    } else {
        Serial.println("Error al crear el archivo inicial.");
    }
}

void setupTWAI() {
    // Configuración fija para 250 kbps
    Serial.println("Configurando TWAI a 250 kbps...");
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("Driver TWAI instalado correctamente.");
        if (twai_start() == ESP_OK) {
            Serial.println("Driver TWAI iniciado correctamente.");
        } else {
            Serial.println("Error al iniciar el driver TWAI.");
        }
    } else {
        Serial.println("Error al instalar el driver TWAI.");
    }
}

void handle_rx_message(twai_message_t &message) {
    // Parpadeo del LED al recibir mensaje
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);

    // Registrar mensaje recibido con marca temporal
    String logEntry = "Timestamp: ";
    logEntry += String(millis());
    logEntry += " ms, ID: 0x";
    logEntry += String(message.identifier, HEX);
    logEntry += ", Data: ";

    for (int i = 0; i < message.data_length_code; i++) {
        logEntry += String(message.data[i], HEX);
        if (i < message.data_length_code - 1) {
            logEntry += " ";
        }
    }

    logEntry += "\n";

    // Agregar al buffer
    messageBuffer += logEntry;

    // Enviar por Bluetooth
    if (SerialBT.connected()) {
        SerialBT.print(logEntry);
    }

    Serial.printf("Mensaje recibido, ID: 0x%x\n", message.identifier);
}

void writeBufferToSD() {
    if (messageBuffer.length() > 0) {
        twai_stop();
        File file = SD.open(logFilePath.c_str(), FILE_APPEND);
        if (file) {
            file.print(messageBuffer);
            file.close();
            messageBuffer = "";
        } else {
            Serial.println("Error al escribir en la SD.");
        }
        twai_start();
    }
}

void setup() {
    Serial.begin(115200);

    // Inicializar LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Inicializar Bluetooth
    SerialBT.begin("ESP32_CAN_Logger"); // Nombre del dispositivo Bluetooth
    Serial.println("Bluetooth inicializado. Listo para emparejar.");

    initSDCard();
    setupTWAI();
}

void loop() {
    static unsigned long lastWriteTime = 0;

    // Leer mensajes TWAI
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
        handle_rx_message(message);
    } else {
        Serial.println("Esperando mensajes...");
    }

    // Escribir datos cada 5 segundos
    if (millis() - lastWriteTime > 5000) {
        writeBufferToSD();
        lastWriteTime = millis();
    }
}
