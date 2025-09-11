#pragma once
#include <Arduino.h>

// ================== IDs CAN ==================
#define CAN_ID_HORA 0x510
#define CAN_ID_SOC  0x541

// ================== Init / estado ==================
void initCAN();
bool isTWAIStarted();
void logTWAIStatus(const char* tag = "TWAI");

// ================== Utilidad de RX rápida (p. ej. esperar SoC en setup) ==================
void checkCANInput();

// ================== Tareas ==================
// Tarea de RX (SoC 0x541). La lanzas desde setup().
void taskCANProcessing(void* pvParameters);

// Arranque de tareas de hora (ticker + envío 200 ms), idempotente
void startHourTasks();

// ================== API de hora (encapsulada) ==================
// Sembrar la hora inicial desde setup() tras leer módem (evita 00:00 al arrancar TX)
void time_seed_from_setup();

// Lectura/escritura segura del par hora:minuto (no toques localHour/minute a pelo)
bool time_ready();                         // ¿hay hora válida lista para enviar?
void time_get_hm(uint8_t& h, uint8_t& m);  // snapshot actual
void time_set_hm(uint8_t h, uint8_t m);    // único punto para escribir h:m

// ================== Envío por CAN ==================
// Versión “blindada”: siempre toma snapshot interno y no envía si no hay hora válida
void sendHourViaCAN_now();

// (opcional) versión clásica por compatibilidad; evita usarla desde tareas periódicas
void sendHourViaCAN(int hour, int minute);
