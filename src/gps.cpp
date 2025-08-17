#include "gps.h"
#include "modem.h"
#include "config.h"
#include "logs.h"
#include <Arduino.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
}

// ===== Configuración =====
// Para evitar que las URCs del GNSS rompan Telegram, por defecto están desactivadas.
// Si quieres ver satélites en progreso, pon 1 y PAUSA/REANUDA URCs desde telegram.cpp.
#define GNSS_USE_URC 0

// Debug opcional de +CGNSINF
// #define GPS_DEBUG_CGNSINF 1

// --------- (Opcional) Mutex global para el canal AT ---------
// Si tu proyecto declara un 'SemaphoreHandle_t at_mutex' global, lo usamos (cooperativo).
// Si no, este weak evita errores de link (no tomará nada).
#ifdef __GNUC__
__attribute__((weak)) SemaphoreHandle_t at_mutex = nullptr;
#else
SemaphoreHandle_t at_mutex = nullptr;
#endif

struct AtGuard {
  bool locked = false;
  AtGuard() {
    if (at_mutex) { xSemaphoreTake(at_mutex, portMAX_DELAY); locked = true; }
  }
  ~AtGuard() {
    if (locked) xSemaphoreGive(at_mutex);
  }
};

// ===== Helpers =====

// Control centralizado de URCs (no-op si GNSS_USE_URC == 0)
static bool gnss_urc_enabled = false;
static void gnssSetURC(bool enable) {
#if GNSS_USE_URC
  if (gnss_urc_enabled == enable) return;
  modem.sendAT(String("+CGNSURC=") + (enable ? "1" : "0"));
  modem.waitResponse(200);
  gnss_urc_enabled = enable;
#else
  (void)enable;
#endif
}

// Exportables para pausar/reanudar desde telegram.cpp (no-op si GNSS_USE_URC == 0)
extern "C" void gnss_pause_urc()  { gnssSetURC(false); }
extern "C" void gnss_resume_urc() { gnssSetURC(true);  }

// Lee la respuesta de +CGNSINF y extrae lat/lon, satélites y HDOP si hay info.
// Devuelve true si hay fix (lat/lon válidos).
static bool parseCGNSINF(String &lat, String &lon, int &sats_used, int &sats_view, float &hdop_out) {
  sats_used = -1;
  sats_view = -1;
  hdop_out  = -1.0f;

  unsigned long t0 = millis();
  // Ventana ampliada para no perder la línea de respuesta
  while (millis() - t0 < 2000) {
    if (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      line.trim();

#ifdef GPS_DEBUG_CGNSINF
      if (line.startsWith("+CGNSINF:")) Serial.println(line);
#endif

      if (line.startsWith("+CGNSINF:")) {
        int idx = line.indexOf(':');
        String payload = (idx >= 0) ? line.substring(idx + 1) : line;
        payload.trim();

        // Tokeniza por comas
        const int MAX_TOK = 32;
        String tok[MAX_TOK];
        int ti = 0, start = 0;
        for (int i = 0; i < payload.length() && ti < MAX_TOK; ++i) {
          if (payload[i] == ',') {
            tok[ti++] = payload.substring(start, i);
            start = i + 1;
          }
        }
        if (ti < MAX_TOK) tok[ti++] = payload.substring(start);

        // 0 run, 1 fix, 2 utc, 3 lat, 4 lon, 5 alt, 6 sog, 7 cog, 8 mode,
        // 9 res1, 10 hdop, 11 pdop, 12 vdop, 13 res2, 14 sats_in_view, 15 sats_used
        if (ti >= 11 && tok[10].length()) hdop_out = tok[10].toFloat();
        if (ti >= 15 && tok[14].length()) sats_view = tok[14].toInt();
        if (ti >= 16 && tok[15].length()) sats_used = tok[15].toInt();

        if (ti >= 5) {
          String fix  = tok[1]; fix.trim();
          String sLat = tok[3]; sLat.trim();
          String sLon = tok[4]; sLon.trim();

          if (fix == "1" && sLat.length() && sLon.length()
              && sLat != "0.000000" && sLon != "0.000000") {
            lat = sLat;
            lon = sLon;
            return true;
          }
        }
      } else if (line == "OK" || line == "ERROR") {
        break;
      }
    } else {
      vTaskDelay(1); // cede CPU mientras no hay datos
    }
  }
  return false;
}

#if GNSS_USE_URC
// Lee URCs +CGNSURC:"GSV"/"GSA" para satélites en vista/usados y HDOP
static void drainGNSSURC(int &last_view, int &last_used, float &last_hdop) {
  int lines = 0; // tope por ciclo para no monopolizar CPU
  while (modem.stream.available() && lines < 10) {
    String line = modem.stream.readStringUntil('\n');
    line.trim();
    if (!line.startsWith("+CGNSURC:")) { lines++; continue; }

    String up = line; up.toUpperCase();

    // ---- GSV: total de satélites en vista es el 4º campo NMEA ($xxGSV)
    int pos = up.indexOf("GSV,");
    if (pos >= 0) {
      String p = line.substring(pos + 4);
      int c1 = p.indexOf(',');
      int c2 = (c1>=0)? p.indexOf(',', c1+1) : -1;
      int c3 = (c2>=0)? p.indexOf(',', c2+1) : -1;
      if (c3 > 0) {
        int v = p.substring(c2+1, c3).toInt();
        if (v > 0) last_view = v;
      }
      lines++; vTaskDelay(1); continue;
    }

    // ---- GSA: sv1..sv12 no vacíos = usados; HDOP es el penúltimo campo
    pos = up.indexOf("GSA,");
    if (pos >= 0) {
      String p = line.substring(pos + 4);
      int used = 0, start = 0, idx = 0;
      for (int i = 0; i <= p.length(); ++i) {
        if (i == p.length() || p[i] == ',') {
          String f = p.substring(start, i); f.trim();
          // 0=op,1=fix, 2..13=sv1..sv12, 14=PDOP, 15=HDOP, 16=VDOP
          if (idx >= 2 && idx <= 13 && f.length()) used++;
          if (idx == 15 && f.length()) {
            float hd = f.toFloat();
            if (hd > 0) last_hdop = hd;
          }
          idx++; start = i + 1;
        }
      }
      if (used >= 0) last_used = used;
      lines++; vTaskDelay(1); continue;
    }

    lines++; vTaskDelay(1);
  }
}
#endif // GNSS_USE_URC

// Fallback por red (CLBS). Devuelve true si obtiene lat/lon y precisión (m).
static bool getApproxLocationCLBS(String &lat, String &lon, int &acc_m, unsigned long timeout_ms=30000) {
  acc_m = -1;
  lat = ""; lon = "";
  modem.sendAT("+CLBS=1,1");
  unsigned long t0 = millis();
  while (millis() - t0 < timeout_ms) {
    if (modem.stream.available()) {
      String line = modem.stream.readStringUntil('\n');
      line.trim();
      if (line.startsWith("+CLBS:")) {
        // +CLBS: <err>,<lat>,<lon>,<acc>   0=OK (a veces lat/lon invertidos)
        int colon = line.indexOf(':');
        String payload = (colon>=0) ? line.substring(colon+1) : line;
        payload.trim();
        String tok[6]; int ti=0, s=0;
        for (int i=0; i<payload.length() && ti<5; ++i) {
          if (payload[i]==',') { tok[ti++]=payload.substring(s,i); s=i+1; }
        }
        tok[ti++] = payload.substring(s);
        if (ti >= 4) {
          int err = tok[0].toInt();
          if (err == 0) {
            String a = tok[1]; a.trim();
            String b = tok[2]; b.trim();
            int acc  = tok[3].toInt();
            double va = a.toDouble();
            double vb = b.toDouble();
            bool a_is_lat = (va >= -90.0 && va <= 90.0);
            bool b_is_lon = (vb >= -180.0 && vb <= 180.0);
            if (a_is_lat && b_is_lon) { lat=a; lon=b; }
            else { lat=b; lon=a; } // invertidos por firmware
            acc_m = (acc > 0 ? acc : -1);
            return (lat.length() && lon.length());
          }
        }
      } else if (line == "ERROR") {
        break;
      }
    } else {
      vTaskDelay(1);
    }
  }
  return false;
}

// ===== API =====

bool obtenerGPS(String &respuesta,
                unsigned long timeout_ms,
                GpsProgressCb progress_cb,
                void* progress_ctx,
                uint16_t progress_step_s) {
  AtGuard lock;   // Toma el mutex AT si existe (cooperativo)

  respuesta = "";

  // --- Logging: sesión GPS completa ---
  logMark("GPS_ON");
  startWindow("GPS_FIX_SEARCH");
  logMsg(LOG_INFO, "GPS", String("Arrancando GNSS; timeout_s=") + String((timeout_ms+500)/1000));

  // Encender GNSS (sin URCs por defecto para no romper Telegram)
  modem.sendAT("+CGNSPWR=1"); modem.waitResponse(500);
  gnssSetURC(false); // asegura URCs OFF si GNSS_USE_URC==0
  modem.sendAT("+CGNSSEQ=\"RMC,GSV,GSA\""); modem.waitResponse(800);

  String lat, lon;
  bool got = false;
  unsigned long start = millis();
  uint16_t total_s = (timeout_ms + 500) / 1000;
  uint16_t last_notified_s = 0;

  int   last_sats_used = -1, last_sats_view = -1;
  float last_hdop      = -1.0f;

#if GNSS_USE_URC
  gnssSetURC(true);  // solo si activaste URCs arriba
#endif

  while (millis() - start < timeout_ms && !got) {
#if GNSS_USE_URC
    // 1) Vacía URCs para ir actualizando satélites/HDOP aunque no haya fix
    drainGNSSURC(last_sats_view, last_sats_used, last_hdop);
#endif

    // 2) Snapshot con +CGNSINF
    modem.sendAT("+CGNSINF");
    int sats_used = -1, sats_view = -1; float hdop = -1.0f;
    got = parseCGNSINF(lat, lon, sats_used, sats_view, hdop);

    if (sats_used >= 0) last_sats_used = sats_used;
    if (sats_view >= 0) last_sats_view = sats_view;
    if (hdop      >= 0) last_hdop      = hdop;

    // Progreso
    uint16_t elapsed_s = (millis() - start + 500) / 1000;
    if (progress_cb &&
        elapsed_s >= last_notified_s + progress_step_s &&
        elapsed_s < total_s) {
      last_notified_s = elapsed_s;
      progress_cb(progress_ctx, elapsed_s, total_s, last_sats_used, last_sats_view);
      // Log ligero de progreso (DEBUG para no ensuciar mucho)
      logMsg(LOG_DEBUG, "GPS",
             String("progreso: ") + elapsed_s + "/" + total_s +
             "s, sats_used=" + (last_sats_used>=0?String(last_sats_used):"N/A") +
             ", sats_view=" + (last_sats_view>=0?String(last_sats_view):"N/A"));
    }

    // Espera entre intentos (cede CPU)
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  String approx_lat, approx_lon;
  int approx_acc = -1;
  bool approx_ok = false;

  if (got) {
    endWindow("GPS_FIX_SEARCH");
    logMsg(LOG_INFO, "GPS", String("FIX OK; sats_used=") +
                      (last_sats_used>=0?String(last_sats_used):"N/A") +
                      " sats_view=" + (last_sats_view>=0?String(last_sats_view):"N/A") +
                      (last_hdop>=0? String(" hdop=")+String(last_hdop,1):""));

    // OK: posición GNSS precisa
    respuesta  = "📍 Ubicación: https://maps.google.com/?q=" + lat + "," + lon;
    respuesta += "\n🛰️ Satélites: " + String((last_sats_used>=0)?last_sats_used:0) +
                 " usados / " + String((last_sats_view>=0)?last_sats_view:0) + " en vista";
    if (last_hdop >= 0) respuesta += "\n🎯 HDOP: " + String(last_hdop, 1);

    // (Opcional) registrar en SD
    // logUbicacion(lat, lon);

  } else {
    endWindow("GPS_FIX_SEARCH");
    // Sin fix → intenta CLBS (aprox. por red)
    startWindow("GPS_CLBS");
    logMsg(LOG_WARN, "GPS", "Sin FIX en timeout; intentando CLBS…");
    approx_ok = getApproxLocationCLBS(approx_lat, approx_lon, approx_acc, 30000);
    endWindow("GPS_CLBS");

    if (approx_ok) {
      logMsg(LOG_INFO, "GPS", String("CLBS OK; acc≈") + (approx_acc>0?String(approx_acc):"N/A") + " m");
      respuesta  = "📍 Ubicación (aprox.): https://maps.google.com/?q=" + approx_lat + "," + approx_lon;
      if (approx_acc > 0) respuesta += "\n±" + String(approx_acc) + " m";
      respuesta += "\nℹ️ Sin fix GNSS en " + String(total_s) + " s; posición por red móvil.";
    } else {
      logMsg(LOG_WARN, "GPS", "CLBS falló; no se pudo obtener ubicación");
      respuesta = "❌ No he podido obtener fix en " + String(total_s) +
                  " s. Inténtalo al aire libre.";
    }
  }

#if GNSS_USE_URC
  gnssSetURC(false);
#endif
  modem.sendAT("+CGNSPWR=0"); modem.waitResponse(500);
  logMark("GPS_OFF");

  return got || approx_ok;
}

void checkGPSStatus(unsigned long /*now*/) {
  // Seguimiento continuo si se necesitara (no bloqueante)
  // Puedes añadir logs aquí si activas un modo "tracking".
}

void powerOffGPS() {
  modem.sendAT("+CGNSPWR=0");
  modem.waitResponse(300);
  logMsg(LOG_INFO, "GPS", "Power OFF solicitado");
}
