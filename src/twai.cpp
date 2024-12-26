//#include "twai.h"
//#include "globals.h"
//#include "output.h"
//#include "messages.h"
//
//QueueHandle_t canQueue;
//const int rx_queue_size = 50;  // Tamaño de la cola
//
//// Configuración global
//twai_general_config_t g_config;
//twai_timing_config_t t_config;
//twai_filter_config_t f_config;
//
//// Inicialización del TWAI (CAN Bus)
//void initializeTWAI() {
//    logToOutputln(INIT_TWAI);
//
//    // Configuración general
//    g_config = {
//        .mode = TWAI_MODE_LISTEN_ONLY,   // Configura el modo de solo escucha
//        .tx_io = GPIO_NUM_32,   // Pin TX cambiado a 32
//        .rx_io = GPIO_NUM_33,   // Pin RX cambiado a 33
//        .clkout_io = TWAI_IO_UNUSED,
//        .bus_off_io = TWAI_IO_UNUSED,
//        .tx_queue_len = 10,
//        .rx_queue_len = 10,
//        .alerts_enabled = 0,  // No se necesitan alertas
//        .clkout_divider = 0,  // Sin salida de reloj
//        .intr_flags = 0,      // Sin interrupciones
//    };
//
//    t_config = TWAI_TIMING_CONFIG_250KBITS();  // Configuración inicial de 250 kbps
//    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Acepta todos los mensajes
//
//    // Instalar el driver TWAI
//    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
//        logToOutputln(INIT_TWAI_INSTALL_OK);
//        driver_installed = true;
//    } else {
//        logToOutputln(INIT_TWAI_INSTALL_ERROR);
//        driver_installed = false;
//        return;
//    }
//
//canQueue = xQueueCreate(rx_queue_size, sizeof(twai_message_t));
//if (canQueue == NULL) {
//    logToOutputln("Error al crear la cola");
//}
//
//    // Iniciar el driver TWAI
//    if (twai_start() == ESP_OK) {
//        logToOutputln(INIT_TWAI_SUCCESS);
//    } else {
//        logToOutputln(INIT_TWAI_ERROR);
//        driver_installed = false;
//    }
//}
//
//// Procesar mensajes TWAI recibidos
//void processTWAIMessage(twai_message_t &message) {
//
//}
//
//// Apagar el TWAI
//void shutdownTWAI() {
//    logToOutputln(TWAI_SHUTDOWN);
//    if (twai_stop() == ESP_OK) {
//        logToOutputln(TWAI_STOP_OK);
//    } else {
//        logToOutputln(TWAI_STOP_ERROR);
//    }
//
//    if (twai_driver_uninstall() == ESP_OK) {
//        logToOutputln(TWAI_UNINSTALL_OK);
//    } else {
//        logToOutputln(TWAI_UNINSTALL_ERROR);
//    }
//}
//
//// Manejo de alertas TWAI
//void handleTWAIAlerts() {
//    if (!driver_installed) return;
//
//    uint32_t alerts_triggered;
//    twai_status_info_t twai_status;
//
//    // Leer alertas generadas
//    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS));
//    twai_get_status_info(&twai_status);
//
//    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
//        logToOutputln(INIT_TWAI_ALERT_ERR_PASS);
//    }
//
//    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
//        logToOutputln(INIT_TWAI_ALERT_BUS_ERROR);
//        logToOutput(INIT_TWAI_ERROR_COUNT);
//        logToOutputln(String(twai_status.bus_error_count));
//    }
//
//    if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
//        logToOutputln(INIT_TWAI_ALERT_RX_QUEUE_FULL);
//        logToOutput(INIT_TWAI_MISSED_MSG);
//        logToOutputln(String(twai_status.rx_missed_count));
//    }
//
//    if (alerts_triggered & TWAI_ALERT_RX_DATA) {
//        twai_message_t message;
//        while (twai_receive(&message, 0) == ESP_OK) {
//            processTWAIMessage(message);
//        }
//    }
//}
