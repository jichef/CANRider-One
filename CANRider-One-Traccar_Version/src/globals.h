#ifndef GLOBALS_H
#define GLOBALS_H

#define TINY_GSM_MODEM_SIM7000 // Define el modelo de módem aquí

#include <Arduino.h>
#include "messages.h"  // Incluir los mensajes
#include "driver/twai.h"

// Variables globales para datos del programa
extern String FINALLATI;
extern String FINALLOGI;
extern String FINALSPEED;
extern String FINALALT;
extern String FINALACCURACY;
extern String FINALIGNITION;
extern String ignition;
extern float battery;
extern float batterylevel;
extern float lat;  
extern float lon;  
extern int localHour;
extern int minute;



// Credenciales GSM
extern const char apn[];
extern const char gprsUser[];
extern const char gprsPass[];
extern const char GSM_PIN[]; 

// Información del servidor Traccar
extern const char server[];
extern String myid;

// Número de teléfono para SMS
extern String phoneNumber;


// Configuración de pines del módem
extern const int PWR_PIN;
extern const int PIN_RX;
extern const int PIN_TX;
extern const int UART_BAUD;

// Configuración de pines del ADC para batería
extern const int BAT_ADC;

// Variables relacionadas con tiempos
extern unsigned long lastSignalCheck;
extern unsigned long noCoverageTime;
extern unsigned long lastReadTime;
extern unsigned long VEHIEncendidoDelay; // Tiempo de espera cuando la moto está encendida
extern unsigned long VEHIApagadoDelay;  // Tiempo de espera cuando la moto está apagada

// Variables globales adicionales

extern bool driver_installed;

// Constantes relacionadas con tiempos
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 60                // Tiempo en segundos para modo sleep
#define POLLING_RATE_MS 1000            // Intervalo para revisar el bus CAN
#define COVERAGE_CHECK_INTERVAL 10000   // Intervalo para verificar cobertura (ms)
#define MAX_NO_COVERAGE_TIME 300000     // Tiempo máximo sin cobertura antes de reiniciar (ms)

// Prototipos de funciones globales
float ReadBattery();

void reboot();

// Palabras clave para comandos recibidos por SMS
extern const char SMS_KEYWORD_GPS[];
extern const char SMS_KEYWORD_REBOOT[];
extern const char SMS_KEYWORD_SECURITY[];
extern const char SMS_KEYWORD_TFT[];
extern const char btDeviceName[];
 
#endif // GLOBALS_H
