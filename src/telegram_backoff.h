#pragma once
#include <Arduino.h>
#include "TinyGsmClientSIM7000SSL.h"   // Necesario para el tipo anidado
#include <UniversalTelegramBot.h>

// ===== Externs del proyecto (definidos en tus otros módulos) =====
extern TinyGsmSim7000SSL modem;
extern TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient;
extern UniversalTelegramBot bot;
extern bool gprsConnected;
extern SemaphoreHandle_t modemMutex;

// Estado del bot (lo mantiene telegram.cpp)
extern volatile bool tgReady;

// Para arbitraje con otros módulos (GPS, etc.)
extern "C" bool telegram_using_modem();

// Opcional: tu función preferida para reconectar GPRS
extern bool connectGPRS();

// ===== Ajustes =====
#ifndef TG_BACKOFF_BASE_MS
#define TG_BACKOFF_BASE_MS   1500UL     // base del backoff
#endif
#ifndef TG_BACKOFF_MAX_MS
#define TG_BACKOFF_MAX_MS    120000UL   // 2 min máx entre intentos
#endif
#ifndef TG_FAILS_GPRS_RESET
#define TG_FAILS_GPRS_RESET  5          // al 5º fallo: reset GPRS
#endif
#ifndef TG_FAILS_MODEM_RESTART
#define TG_FAILS_MODEM_RESTART 8        // al 8º fallo: restart módem
#endif
#ifndef TG_FAILS_ESP_RESTART
#define TG_FAILS_ESP_RESTART  12        // al 12º fallo: ESP.restart()
#endif
#ifndef TG_STALE_OK_MS
#define TG_STALE_OK_MS       (30UL*60UL*1000UL) // 30 min sin éxito => agresivo
#endif

// ===== API =====
void     telegram_backoff_reset();
uint16_t telegram_fail_count();
uint32_t telegram_last_ok_ms();
uint32_t telegram_next_try_eta_ms();

// Devuelve true si getMe() OK y deja tgReady=true. Respeta backoff y escalado.
bool     telegram_init_step(uint32_t now_ms);
