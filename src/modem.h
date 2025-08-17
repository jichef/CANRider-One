#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t modemMutex;   // ← declaración global

#include <TinyGsmClientSIM7000SSL.h>
#include <UniversalTelegramBot.h>
extern bool gprsConnected;  // ← solo declaración, NUNCA con “= …”

extern TinyGsmSim7000SSL modem;
extern TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient;
extern UniversalTelegramBot bot;


bool ensureDataAttached();        // <-- prototipo
void taskNetWatchdog(void *pv);   // <-- prototipo

// --- Inicialización / red ---
void initModem();                                  // <- usada en main.ino
bool connectGPRS();                                // configura APN, DNS, TLS, NTP

// --- Utilidades de red (internas pero expuestas por si las usas) ---
void setNetworkModeDIGI();                         // CNMP/CMNB
void applyDnsPdpAndTls(const char* apn);           // CDNSCFG/CGDCONT/CSSLCFG/SNI
bool syncModemTimeNTP();                           // CNTP + verificación CCLK?

// --- Hora desde módem ---
bool getTimeFromModem(int &hour, int &minute);     // <- usada en main.ino y can_bus.cpp
