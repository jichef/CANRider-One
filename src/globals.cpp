#include "globals.h"
#include "Arduino.h"
#include "output.h"

// Nombre por defecto del dispositivo Bluetooth
const char btDeviceName[] = "CANRider One"; 

// Credenciales GSM
const char apn[] = ""; 
const char gprsUser[] = "";
const char gprsPass[] = "";
const char GSM_PIN[] = ""; 

// Información del servidor Traccar
const char server[] = ".duckdns.org"; // Tu servidor de Traccar
String myid = "**************************"; // ID configurada en Traccar

// Tiempos de actualización
unsigned long VEHIEncendidoDelay = 10000; // Tiempo para actualizar ubicación GPS y enviar a Traccar (en milisegundos; cuando hay alimentación USB)
unsigned long VEHIApagadoDelay = 900000;  // Tiempo para actualizar ubicación GPS y enviar a Traccar (en milisegundos; cuando NO hay alimentación USB)

// Palabras clave para comandos recibidos por SMS

String phoneNumber = ""; 			// Número de teléfono autorizado para SMS (con prefijo internacional)

const char SMS_KEYWORD_SECURITY[] = "********";		// Palabra de seguridad para SMS
const char SMS_KEYWORD_GPS[] = "gps";		     	// Comando para obtener localización GPS por SMS, en minúsculas
const char SMS_KEYWORD_REBOOT[] = "reboot";		    // Comando para forzar reinicio por SMS, en minúsculas

// Pines del módem
const int PWR_PIN = 4;

// Pin ADC para batería
const int BAT_ADC = 35;

// Variables globales para datos del programa
String FINALLATI = "0";
String FINALLOGI = "0";
String FINALSPEED = "0";
String FINALALT = "0";
String FINALACCURACY = "0";
String FINALIGNITION = "false";
String ignition = "false";
float battery = 0.0;

// Configuración del módem
const int PIN_RX = 26;     // Pin RX conectado al TX del módem
const int PIN_TX = 27;     // Pin TX conectado al RX del módem
const int UART_BAUD = 115200; // Velocidad de comunicación

// Variables relacionadas con tiempos
unsigned long lastSignalCheck = 0;
unsigned long noCoverageTime = 0;
unsigned long lastReadTime = 0;

// Variables globales adicionales
int batterylevel = 0;
bool driver_installed = false;

// Función para leer el voltaje de la batería
float ReadBattery() {
    float vref = 1.100;
    uint16_t volt = analogRead(BAT_ADC);
    float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * vref;
    return battery_voltage;
}

void reboot() {
    ESP.restart(); // Reinicia el ESP32
}
