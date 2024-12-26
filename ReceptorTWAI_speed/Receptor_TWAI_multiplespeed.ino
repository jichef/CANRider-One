#include <driver/twai.h>

// Definir una cola de FreeRTOS similar al primer código
QueueHandle_t canQueue;
const int rx_queue_size = 10;  // Tamaño de la cola

unsigned long previousMillis = 0;  // Variable para controlar el tiempo
const long interval = 5000;         // Intervalo de 5 segundos para cambiar la velocidad CAN
int speedIndex = 0;                 // Índice para cambiar entre las velocidades
const int speeds[] = {125, 250, 500, 1000};  // Velocidades disponibles

// Configuración global
twai_general_config_t g_config;
twai_timing_config_t t_config;
twai_filter_config_t f_config;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // Espera a que el puerto serial esté listo
    }

    Serial.println("Configuración inicial CAN");

    // Configuración del bus CAN (pines TX y RX)
    g_config = {
        .mode = TWAI_MODE_NORMAL,
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

    // Configuración inicial de la velocidad del bus CAN (250 kbps)
    t_config = TWAI_TIMING_CONFIG_250KBITS();  // Configuración a 250 kbps
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Acepta todos los mensajes

    // Inicializa el bus CAN
    twai_driver_install(&g_config, &t_config, &f_config);

    // Crea la cola de FreeRTOS
    canQueue = xQueueCreate(rx_queue_size, sizeof(twai_message_t));

    // Inicia el bus CAN
    twai_start();
    Serial.println("CAN inicializado");
}

void loop() {
    unsigned long currentMillis = millis();

    // Se establece el tiempo de espera similar al primer código (3 ms)
    if (currentMillis - previousMillis >= 3) {
        previousMillis = currentMillis;

        // Recibe los mensajes CAN y los coloca en la cola
        twai_message_t rx_message;
        if (twai_receive(&rx_message, pdMS_TO_TICKS(3)) == ESP_OK) {
            // Enviar el mensaje recibido a la cola
            if (xQueueSend(canQueue, &rx_message, 0) != pdTRUE) {
                Serial.println("Error al agregar mensaje a la cola");
            }
        } else {
            Serial.println("No se recibió mensaje CAN");
        }
    }

    // Procesar los mensajes desde la cola
    twai_message_t rx_message;
    if (xQueueReceive(canQueue, &rx_message, 0) == pdTRUE) {
        // Mostrar el ID del mensaje
        Serial.print("ID: 0x");
        Serial.print(rx_message.identifier, HEX);
        Serial.print(" - Datos: ");
        
        // Mostrar todos los bytes del mensaje
        for (int i = 0; i < rx_message.data_length_code; i++) {
            Serial.print(rx_message.data[i], HEX); // Mostrar cada byte en formato hexadecimal
            Serial.print(" ");
        }
        Serial.println();  // Nueva línea después de mostrar los bytes
    }

    // Cambiar la velocidad de CAN cada intervalo de 5 segundos
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Cambiar a la siguiente velocidad en el arreglo de velocidades
        speedIndex = (speedIndex + 1) % 4;  // Aumentar y volver a 0 después de 1000 kbps

        // Configurar la velocidad según el índice
        switch (speeds[speedIndex]) {
            case 125:
                t_config = TWAI_TIMING_CONFIG_125KBITS();
                Serial.println("Cambiando velocidad a 125 kbps");
                break;
            case 250:
                t_config = TWAI_TIMING_CONFIG_250KBITS();
                Serial.println("Cambiando velocidad a 250 kbps");
                break;
            case 500:
                t_config = TWAI_TIMING_CONFIG_500KBITS();
                Serial.println("Cambiando velocidad a 500 kbps");
                break;
            case 1000:
                t_config = TWAI_TIMING_CONFIG_1MBITS();
                Serial.println("Cambiando velocidad a 1000 kbps");
                break;
        }

        // Detener y reiniciar el driver para aplicar la nueva configuración
        twai_stop();
        twai_driver_uninstall();
        twai_driver_install(&g_config, &t_config, &f_config);
        twai_start();
    }
}
