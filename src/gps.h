#pragma once
#include <Arduino.h>

// Callback de progreso: cada 'progress_step_s' segundos
// sats_used/sats_view = -1 si no disponibles
typedef void (*GpsProgressCb)(void* ctx,
                              uint16_t elapsed_s,
                              uint16_t total_s,
                              int sats_used,
                              int sats_view);

bool obtenerGPS(String &respuesta,
                unsigned long timeout_ms,
                GpsProgressCb progress_cb = nullptr,
                void* progress_ctx = nullptr,
                uint16_t progress_step_s = 5);

void checkGPSStatus(unsigned long now);
void powerOffGPS();
