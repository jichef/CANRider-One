#include "driver/twai.h"

// Pines utilizados para el transceptor CAN
#define RX_PIN 5
#define TX_PIN 4

// Intervalo entre transmisiones en milisegundos
#define POLLING_RATE_MS 100

unsigned long statusPrintMillis = 0;  // Para imprimir el estado del bus
unsigned long lastDecrementMillis = 0; // Para el temporizador de decremento
int currentMessageIndex = 0;       // Índice del mensaje actual
uint8_t firstByteValue = 100; // Valor inicial del primer byte del mensaje 0x541

// Estructura para almacenar mensajes CAN
typedef struct {
  uint32_t id;
  uint8_t length;
} CANMessage;

// Mensajes CAN especificados con IDs
CANMessage messages[] = {
  {0x503, 8},
  {0x505, 8},
  {0x507, 8},
  {0x54F, 8},
  {0x501, 8},
  {0x502, 8},
  {0x510, 8},
  {0x511, 8},
  {0x541, 8},
  {0x530, 8},
};
const int numMessages = sizeof(messages) / sizeof(messages[0]);

bool driver_installed = false;

void generate_random_data(uint8_t *data, uint8_t length) {
  for (int i = 0; i < length; i++) {
    data[i] = random(0, 256); // Generar bytes aleatorios entre 0 y 255
  }
}

void setup() {
  Serial.begin(115200);

  // Configuraciones del driver TWAI
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Instalar el driver TWAI
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver instalado");
  } else {
    Serial.println("Error al instalar driver");
    return;
  }

  // Iniciar el driver TWAI
  if (twai_start() == ESP_OK) {
    Serial.println("Driver iniciado");
  } else {
    Serial.println("Error al iniciar driver");
    return;
  }

  // Reconfigurar alertas para TX y errores
  uint32_t alerts_to_enable = TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    Serial.println("CAN Alerts reconfigurados");
  } else {
    Serial.println("Error al reconfigurar alertas");
    return;
  }
  driver_installed = true;
}

void send_message(CANMessage msg) {
  if (!driver_installed) return;

  twai_message_t message;
  message.identifier = msg.id;
  message.data_length_code = msg.length;
  message.rtr = false; // No es un mensaje de solicitud remota

  // Generar datos aleatorios
  generate_random_data(message.data, msg.length);

  // Si el mensaje es el 0x541, modificamos el primer byte para que no sea mayor a 100
  if (msg.id == 0x541) {
    message.data[0] = firstByteValue;  // Asignar el valor de firstByteValue

    // Para asegurarnos de que se vea correctamente, colocamos un valor predeterminado en los otros bytes
    for (int i = 1; i < msg.length; i++) {
      message.data[i] = 0x00; // Rellenamos el resto de los bytes con un valor predeterminado
    }
  }

  // Transmitir el mensaje
  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Mensaje enviado -> ID: 0x%03X, Data: ", msg.id);
    for (int i = 0; i < msg.length; i++) {
      Serial.printf("%02X ", message.data[i]);
    }
    Serial.println();
  } else {
    Serial.println("Error al enviar mensaje");
  }
}

void print_bus_status() {
  twai_status_info_t status;
  twai_get_status_info(&status);

  Serial.printf("\n=== Estado del Bus CAN ===\n");
  Serial.printf("Estado: %d\n", status.state);
  Serial.printf("Errores TX: %d, RX: %d\n", status.tx_error_counter, status.rx_error_counter);
  Serial.printf("Mensajes TX pendientes: %d\n", status.msgs_to_tx);
  Serial.printf("Mensajes RX pendientes: %d\n", status.msgs_to_rx);
  Serial.println("========================");
}

void loop() {
  // Transmitir mensajes constantemente
  send_message(messages[currentMessageIndex]);
  currentMessageIndex = (currentMessageIndex + 1) % numMessages;

  delay(10); // Controla la frecuencia de transmisión

  // Imprimir estado del bus cada 5 segundos
  unsigned long currentMillis = millis();
  if (currentMillis - statusPrintMillis >= 5000) {
    statusPrintMillis = currentMillis;
    print_bus_status();
  }

  // Disminuir el primer byte del mensaje 0x541 cada 5 segundos
  if (currentMillis - lastDecrementMillis >= 5000) {
    lastDecrementMillis = currentMillis;

    // Disminuir el valor del primer byte y reciclar al llegar a 0
    if (firstByteValue > 0) {
      firstByteValue--;
    } else {
      firstByteValue = 100;  // Reiniciar el ciclo al llegar a 0
    }
  }
}
