#pragma once
#include <Arduino.h>

// ==========================================================
// gps.h — API pública para el módulo GPS (SIM7000 + ESP32)
// ==========================================================

// Callback de progreso durante la búsqueda de FIX.
// - ctx: puntero opaco que recibes de vuelta
// - elapsed_s: segundos transcurridos
// - total_s: segundos totales de timeout
// - sats_used: satélites usados en la solución (o -1 si N/D)
// - sats_view: satélites en vista (o -1 si N/D)
typedef void (*GpsProgressCb)(void* ctx,
                              uint16_t elapsed_s,
                              uint16_t total_s,
                              int sats_used,
                              int sats_view);

// ---------- Control de energía / sesión ----------
bool gpsStartFor(uint32_t ms);      // Enciende el GNSS por 'ms' (renueva si ya estaba activo)
void gpsKill();                     // Apaga GNSS inmediatamente y cancela temporizador
bool gpsActive();                   // ¿GNSS encendido?

// ---------- Estado de fix ----------
bool gpsFixInProgress();            // ¿Se está intentando obtener FIX ahora mismo?
void gpsRequestAbort();             // Solicitud cooperativa de abortar el intento de FIX

// ---------- Adquisición de posición ----------
// Devuelve true si obtuvo FIX GNSS o ubicación aproximada por red (CLBS).
// - respuesta: texto listo para Telegram (URL + info)
// - timeout_ms: tiempo máximo buscando FIX
// - progress_cb/progress_ctx: callback opcional de progreso
// - progress_step_s: intervalo de notificación en segundos
bool obtenerGPS(String &respuesta,
                unsigned long timeout_ms,
                GpsProgressCb progress_cb = nullptr,
                void* progress_ctx = nullptr,
                uint16_t progress_step_s = 5);

// ---------- Hook opcional de ciclo ----------
void checkGPSStatus(unsigned long now);   // actualmente no-op
