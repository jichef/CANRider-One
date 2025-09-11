// gps.cpp
#include "gps.h"
#include "modem.h"
#include "config.h"
#include "logs.h"
#include <Arduino.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
  #include "freertos/timers.h"
}

// ==== Señales hacia/desde Telegram ====
extern "C" bool telegram_using_modem();
bool telegramSend(const String& msg);              // lo define telegram.cpp
bool telegramFlushQuick(uint16_t timeout_ms=1200); // lo define telegram.cpp

// ===== Configuración =====
#define GNSS_USE_URC 0                // 0 = sin URCs NMEA; 1 = con URCs (GSV/GSA)
#define GNSS_READ_SLEEP_MS 1000
#define GPS_LED_PIN 12
#define GPS_LED_ACTIVE_LOW 1
#define REFINEMENT_NOTIFY_EVERY_MS 25000UL
#define PROGRESS_CB_STEP_S 30         // típicamente lo pasas desde telegram (30 s)

// ===== Forward de modemLock/modemUnlock (las implementas en otro módulo)
bool modemLock(const char* tag, uint32_t ms);
void modemUnlock(const char* tag);

// ===== Estado de GPS para /gps (temporizado) =====
static volatile bool s_gpsActive   = false;   // GNSS encendido
static volatile bool s_timerMode   = false;   // GNSS por temporizador (/gps)
static TimerHandle_t s_gpsTimer    = nullptr;

// ===== Estado: intento de FIX en curso =====
static volatile bool s_fixInProgress = false;
bool gpsFixInProgress() { return s_fixInProgress; }

// ===== Abort inmediato del intento de fix =====
static volatile bool s_abortReq = false;
void gpsRequestAbort() { s_abortReq = true; }

// ---------------- Helpers ----------------
static bool lockWithArbitration(const char* tag, uint32_t total_ms, uint32_t slice_ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < total_ms) {
    if (telegram_using_modem()) { vTaskDelay(pdMS_TO_TICKS(80)); continue; }
    if (modemLock(tag, slice_ms)) return true;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return false;
}

static void gpsLedSet(bool on) {
#if defined(GPS_LED_PIN)
  static bool inited = false;
  if (!inited) {
    pinMode(GPS_LED_PIN, OUTPUT);
  #ifdef GPS_LED_ACTIVE_LOW
    digitalWrite(GPS_LED_PIN, HIGH);
  #else
    digitalWrite(GPS_LED_PIN, LOW);
  #endif
    inited = true;
  }
  #ifdef GPS_LED_ACTIVE_LOW
    digitalWrite(GPS_LED_PIN, on ? LOW : HIGH);
  #else
    digitalWrite(GPS_LED_PIN, on ? HIGH : LOW);
  #endif
  logMsg(LOG_INFO, "GPS_LED", String("GPIO ") + GPS_LED_PIN + " -> " + (on ? "ON" : "OFF"));
#else
  (void)on;
#endif
}

// Control URCs (solo si GNSS_USE_URC=1)
static bool gnss_urc_enabled = false;
static void gnssSetURC(bool enable) {
#if GNSS_USE_URC
  if (gnss_urc_enabled == enable) return;
  if (!lockWithArbitration("GPS_URC", 600, 300)) {
    logMsg(LOG_DEBUG, "MUTEX", "GPS no pudo tomar el mutex (URC)");
    return;
  }
  modem.sendAT(String("+CGNSURC=") + (enable ? "1" : "0"));
  modem.waitResponse(200);
  modemUnlock("GPS_URC");
  gnss_urc_enabled = enable;
#else
  (void)enable;
#endif
}

// Encendido/apagado
static bool powerOnGPS() {
  if (!lockWithArbitration("GPS_PWR", 1200, 300)) {
    logMsg(LOG_WARN, "MUTEX", "GPS no pudo tomar el mutex (power ON)");
    return false;
  }
  modem.sendAT("+SGPIO=0,4,1,1"); modem.waitResponse(500);     // LNA/ANT
  modem.sendAT("+CGNSPWR=1");     modem.waitResponse(600);
  modem.sendAT("+CGNSSEQ=\"RMC,GSV,GSA\""); modem.waitResponse(400);
  modem.sendAT("+CGNSURC=0");     modem.waitResponse(200);
  modemUnlock("GPS_PWR");

  gnssSetURC(false);
  s_gpsActive = true;
  s_abortReq  = false;
  gpsLedSet(true);
  logMsg(LOG_INFO, "GPS", "GNSS encendido");
  return true;
}

void powerOffGPS() {
  for (int i=0; i<5 && telegram_using_modem(); ++i) vTaskDelay(pdMS_TO_TICKS(120));
  if (!lockWithArbitration("GPS_PWR", 900, 300)) {
    logMsg(LOG_WARN, "MUTEX", "GPS no pudo tomar el mutex (power OFF)");
    return;
  }
  modem.sendAT("+CGNSPWR=0"); modem.waitResponse(300);
  modemUnlock("GPS_PWR");

  s_gpsActive = false;
  s_timerMode = false;
  s_fixInProgress = false;
  gpsLedSet(false);
  logMsg(LOG_INFO, "GPS", "Power OFF solicitado");
}

// Timer callback
static void gpsOffTimerCb(TimerHandle_t) {
  logMsg(LOG_INFO, "GPS", "Timer expirado: apagando GNSS");
  powerOffGPS();
}

// -------------------- PARSEO CGNSINF + NMEA ---------------------
static void parseGSV_Count(const String& nmea, int& sats_view_max) {
  int c = 0, start = 0, sv_in_view = -1;
  for (int i = 0; i < (int)nmea.length(); ++i) {
    if (nmea[i] == ',' || nmea[i] == '*') {
      String f = nmea.substring(start, i); f.trim();
      ++c;
      if (c == 3) { if (f.length()) sv_in_view = f.toInt(); break; }
      start = i + 1;
    }
  }
  if (sv_in_view > sats_view_max) sats_view_max = sv_in_view;
}

static void parseGSA_Used(const String& nmea, int& sats_used) {
  int field = 0, start = 0, count = 0;
  for (int i = 0; i < (int)nmea.length(); ++i) {
    if (nmea[i] == ',' || nmea[i] == '*') {
      ++field;
      if (field >= 4 && field <= 15) { String prn = nmea.substring(start, i); prn.trim(); if (prn.length()) ++count; }
      start = i + 1;
      if (nmea[i] == '*') break;
    }
  }
  if (count > sats_used) sats_used = count;
}

static void parseGSA_HDOP(const String& nmea, float& hdop_out) {
  int star = nmea.indexOf('*');
  String core = (star > 0 ? nmea.substring(0, star) : nmea);
  int last = core.lastIndexOf(',');
  if (last < 0) return;
  int prev = core.lastIndexOf(',', last - 1);
  if (prev < 0) return;
  String hd = core.substring(prev + 1, last);
  hd.trim();
  if (hd.length()) {
    float v = hd.toFloat();
    if (v > 0.0f) hdop_out = v;
  }
}

static bool readCGNSINF_ShortLocked(String &lat, String &lon,
                                    int &sats_used, int &sats_view, float &hdop_out) {
  sats_used = -1; sats_view = -1; hdop_out = -1.0f;
  lat = ""; lon = "";

  // Backoff corto si el módem está ocupado (p. ej., TLS de Telegram)
  if (!lockWithArbitration("GPS_INF", 1200, 300)) {  // +tiempo total 1200 ms
    logMsg(LOG_DEBUG, "MUTEX", "GPS no pudo tomar el mutex (INF)");
    vTaskDelay(pdMS_TO_TICKS(120));                  // <<-- cede CPU y evita WDT
    return false;
  }
  modem.sendAT("+CGNSINF");
  unsigned long t0 = millis();
  bool gotFixLine = false, gotOK = false;
  int view_max = -1, used_max = -1; float gsa_hdop = -1.0f;

  while (millis() - t0 < 500) {
    if (s_abortReq) break;
    if (!modem.stream.available()) { vTaskDelay(1); continue; }
    String line = modem.stream.readStringUntil('\n');
    line.trim();

    if (line.startsWith("+CGNSINF:")) {
      int idx = line.indexOf(':');
      String payload = (idx >= 0) ? line.substring(idx + 1) : line;
      payload.trim();

      const int MAX_TOK = 24;
      String tok[MAX_TOK];
      int ti = 0, start = 0;
      for (int i = 0; i < payload.length() && ti < (MAX_TOK - 1); ++i) {
        if (payload[i] == ',') { tok[ti++] = payload.substring(start, i); start = i + 1; }
      }
      tok[ti++] = payload.substring(start);

      if (ti >= 5) {
        String fix  = tok[1]; fix.trim();
        String sLat = tok[3]; sLat.trim();
        String sLon = tok[4]; sLon.trim();
        if (ti >= 11 && tok[10].length()) hdop_out  = tok[10].toFloat();
        if (ti >= 15 && tok[14].length()) { int v14 = tok[14].toInt(); if (v14 > view_max) view_max = v14; }
        if (ti >= 16 && tok[15].length()) { int v15 = tok[15].toInt(); if (v15 > used_max) used_max = v15; }

        if (fix == "1" && sLat.length() && sLon.length()
            && sLat != "0.000000" && sLon != "0.000000") {
          lat = sLat; lon = sLon; gotFixLine = true;
        }
      }
    } else if (line.startsWith("$GPGSV") || line.startsWith("$GLGSV") ||
               line.startsWith("$GAGSV") || line.startsWith("$GBGSV") ||
               line.startsWith("$GNGSV")) {
      parseGSV_Count(line, view_max);
    } else if (line.startsWith("$GPGSA") || line.startsWith("$GNGSA") ||
               line.startsWith("$GLGSA") || line.startsWith("$GAGSA") ||
               line.startsWith("$GBGSA")) {
      parseGSA_Used(line, used_max);
      parseGSA_HDOP(line, gsa_hdop);
    } else if (line == "OK") {
      gotOK = true; break;
    } else if (line == "ERROR") {
      break;
    }
  }

  modemUnlock("GPS_INF");

  if (hdop_out < 0.0f && gsa_hdop > 0.0f) hdop_out = gsa_hdop;
  if (view_max >= 0) sats_view = view_max;  else if (sats_view < 0) sats_view = 0;
  if (used_max >= 0) sats_used = used_max;  else if (sats_used < 0) sats_used = 0;

  return gotFixLine && gotOK;
}

// Fallback CLBS
static bool getApproxLocationCLBS_ShortLocked(String &lat, String &lon, int &acc_m, unsigned long timeout_ms=15000) {
  acc_m = -1; lat = ""; lon = "";
  unsigned long t0 = millis();
  bool sent = false;

  while (millis() - t0 < timeout_ms) {
    if (s_abortReq) return false;
    if (!lockWithArbitration("GPS_CLBS", 900, 300)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

    if (!sent) { modem.sendAT("+CLBS=1,1"); sent = true; }

    bool done = false;
    unsigned long w0 = millis();
    while (millis() - w0 < 400) {
      if (s_abortReq) { done = true; break; }
      if (!modem.stream.available()) { vTaskDelay(1); continue; }
      String line = modem.stream.readStringUntil('\n'); line.trim();

      if (line.startsWith("+CLBS:")) {
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
            float va = a.toFloat();
            float vb = b.toFloat();
            bool a_is_lat = (va >= -90.0f && va <= 90.0f);
            bool b_is_lon = (vb >= -180.0f && vb <= 180.0f);
            if (a_is_lat && b_is_lon) { lat=a; lon=b; } else { lat=b; lon=a; }
            acc_m = (acc > 0 ? acc : -1);
            modemUnlock("GPS_CLBS");
            return (lat.length() && lon.length());
          }
        }
        done = true;
      } else if (line == "ERROR") {
        done = true;
      }
    }
    modemUnlock("GPS_CLBS");
    if (done) break;
    vTaskDelay(pdMS_TO_TICKS(120));
  }
  return false;
}

// ===== API =====
bool gpsStartFor(uint32_t ms) {
  if (!s_gpsTimer) {
    s_gpsTimer = xTimerCreate("gps_kill", pdMS_TO_TICKS(ms), pdFALSE, nullptr, gpsOffTimerCb);
    if (!s_gpsTimer) {
      logMsg(LOG_WARN, "GPS", "No se pudo crear el timer GNSS");
      return false;
    }
  }
  if (!s_gpsActive) {
    if (!powerOnGPS()) { logMsg(LOG_WARN, "GPS", "Fallo al encender GNSS"); return false; }
  } else {
    gnssSetURC(false);
  }
  s_abortReq = false;
  s_timerMode = true;
  xTimerStop(s_gpsTimer, 0);
  xTimerChangePeriod(s_gpsTimer, pdMS_TO_TICKS(ms), 0);
  xTimerStart(s_gpsTimer, 0);
  logMsg(LOG_INFO, "GPS", String("GNSS activo por ") + (ms/1000) + " s");
  return true;
}

void gpsKill() {
  if (s_gpsTimer) xTimerStop(s_gpsTimer, 0);
  s_abortReq = true;
  s_fixInProgress = false;
  powerOffGPS();
  logMsg(LOG_INFO, "GPS", "gpsKill(): GNSS detenido");
}

bool gpsActive() { return s_gpsActive; }

// --- Clasificación de calidad ---
struct Qtier { const char* name; float hdop_max; int sats_min; const char* emoji; };
static const Qtier kTiers[] = {
  {"Excelente", 1.2f, 9,  "💎"},
  {"Buena",     1.8f, 7,  "✅"},
  {"Aceptable", 2.5f, 5,  "🟡"},
  {"Pobre",     4.0f, 4,  "⚠️"},
  {"Mala",     99.0f, 0,  "❌"},
};
static int classifyTier(float hdop, int used) {
  for (int i=0;i<4;i++) { if (hdop>0 && hdop<=kTiers[i].hdop_max && used>=kTiers[i].sats_min) return i; }
  return 4;
}
// objetivo dinámico según fracción de tiempo
static int targetTierForFrac(float frac) {
  if (frac < 0.30f) return 0;       // 0–30%: Excelente
  if (frac < 0.60f) return 1;       // 30–60%: Buena
  if (frac < 0.85f) return 2;       // 60–85%: Aceptable
  return 3;                         // 85–100%: Pobre
}

// antes: static String buildRefineText(int tierIdx, int used, int view, float hdop, unsigned long remaining_ms)
static String buildRefineText(int tierIdx, int used, int view, float hdop,
                              unsigned long remaining_ms,
                              const String& lat = String(),
                              const String& lon = String(),
                              bool bestFlag = false) {
  String s = "🎯 Refinando precisión — objetivo *";
  s += kTiers[tierIdx].name; s += "* (HDOP ≤ "; s += String(kTiers[tierIdx].hdop_max,1);
  s += ", usados ≥ "; s += kTiers[tierIdx].sats_min; s += ")\n";
  s += "👀 En vista: "; s += String(view>=0?view:0);
  s += " | Usados: ";   s += String(used>=0?used:0);
  s += " | HDOP: ";     s += (hdop>0? String(hdop,1) : String("N/A"));
  s += "\n⏳ Tiempo restante aprox.: ";
  unsigned long sec = (remaining_ms + 500)/1000;
  s += String(sec/60); s += "m "; s += String(sec%60); s += "s";
  if (lat.length() && lon.length()) {
    s += "\n📍 "; s += (bestFlag ? "Mejor hasta ahora" : "Actual (provisional)");
    s += ": https://maps.google.com/?q="; s += lat; s += ","; s += lon;
  }
  return s;
}


bool obtenerGPS(String &respuesta,
                unsigned long timeout_ms,
                GpsProgressCb progress_cb,
                void* progress_ctx,
                uint16_t progress_step_s) {
  respuesta = "";
  logMark("GPS_ON");
  startWindow("GPS_FIX_SEARCH");
  logMsg(LOG_INFO, "GPS", String("Arrancando GNSS; timeout_s=") + String((timeout_ms+500)/1000));

  if (!s_gpsActive) {
    if (!powerOnGPS()) { endWindow("GPS_FIX_SEARCH"); logMsg(LOG_WARN, "GPS", "No se pudo encender GNSS"); return false; }
  } else { gnssSetURC(false); }

#if GNSS_USE_URC
  gnssSetURC(true);
#endif

  s_fixInProgress = true;

  String lat, lon, best_lat, best_lon;
  bool got = false, have_best=false;
  int best_used=0, best_view=0, last_sats_used=0, last_sats_view=0;
  float best_hdop=99.0f, last_hdop=-1.0f;

  unsigned long start = millis();
  const uint16_t total_s = (timeout_ms + 500) / 1000;
  uint16_t last_notified_s = 0;
  unsigned long last_refine_telegram_ms = 0;
  int last_target_idx = -1;

  // Primer recordatorio de objetivo (telegram.cpp ya envía uno; este refuerza con métricas cuando existan)
  (void)telegramSend(buildRefineText(0, 0, 0, -1, timeout_ms));

  logMsg(LOG_INFO, "GPS", "REFINO v2 activo (ticker 5s + callback periódico)");

  while (!s_abortReq && (millis() - start < timeout_ms) && !got) {
    // 1) Progreso por tiempo (independiente del lock)
    uint16_t elapsed_s = (millis() - start + 500) / 1000;
    if (progress_cb && progress_step_s > 0 &&
        elapsed_s >= (uint16_t)(last_notified_s + progress_step_s) &&
        elapsed_s < total_s) {
      last_notified_s = elapsed_s;
      progress_cb(progress_ctx, elapsed_s, total_s, last_sats_used, last_sats_view);
      logMsg(LOG_DEBUG, "GPS", String("progreso: ") + elapsed_s + "/" + total_s +
                         "s, sats_used=" + String(last_sats_used) +
                         " sats_view=" + String(last_sats_view) +
                         (last_hdop>0? String(" hdop=")+String(last_hdop,1) : " hdop=N/A"));
    }

    // 2) Lectura GNSS (locks cortos)
    int su=0, sv=0; float hd=-1.0f;
    String tlat, tlon;
    bool fixNow = readCGNSINF_ShortLocked(tlat, tlon, su, sv, hd);

    if (su > 0) last_sats_used = su;
    if (sv > 0) last_sats_view = sv;
    if (hd > 0) last_hdop = hd;

    const float frac = float(elapsed_s) / float(total_s > 0 ? total_s : 1);
    const int target_idx = targetTierForFrac(frac);
    const int have_idx   = classifyTier((last_hdop>0?last_hdop:99.0f), last_sats_used);

    // 3) Aviso “Refinando precisión” cada ~25 s o si cambia objetivo
  // 3) Aviso “Refinando precisión” cada ~25 s o si cambia objetivo
unsigned long now_ms = millis();
unsigned long remaining_ms = (start + timeout_ms > now_ms) ? (start + timeout_ms - now_ms) : 0UL;

// preferimos incluir coordenadas si hay mejor fix conocido o uno reciente
String refLat, refLon;
bool   refIsBest = false;
if (have_best && best_lat.length() && best_lon.length()) {
  refLat = best_lat; refLon = best_lon; refIsBest = true;
} else if (fixNow && tlat.length() && tlon.length()) {
  refLat = tlat; refLon = tlon; refIsBest = false;
}

if (!got && !s_abortReq &&
    (now_ms - last_refine_telegram_ms >= REFINEMENT_NOTIFY_EVERY_MS || target_idx != last_target_idx)) {
  (void)telegramSend(buildRefineText(target_idx, last_sats_used, last_sats_view, last_hdop,
                                     remaining_ms, refLat, refLon, refIsBest));
  last_refine_telegram_ms = now_ms;
  last_target_idx = target_idx;
}

    // 4) Registrar mejor solución vista (aunque no cumpla objetivo actual)
if (fixNow) {
  int prevTier = have_best ? classifyTier(best_hdop>0?best_hdop:99.0f, best_used) : 4;
  int newTier  = classifyTier(hd>0?hd:99.0f, su);

  bool improved = (!have_best) ||
                  (newTier < prevTier) ||
                  (newTier == prevTier && (
                      (hd > 0 && best_hdop > 0 && hd < best_hdop - 0.3f) || // mejora clara de HDOP
                      (su >= best_used + 2)                                  // +2 sat usados
                  ));

  // actualizar "mejor hasta ahora"
  if (!have_best ||
      classifyTier(hd>0?hd:99.0f, su) < classifyTier(best_hdop>0?best_hdop:99.0f, best_used) ||
      (classifyTier(hd>0?hd:99.0f, su) == classifyTier(best_hdop>0?best_hdop:99.0f, best_used) &&
       hd>0 && (best_hdop<=0 || hd < best_hdop))) {
    best_lat = tlat; best_lon = tlon; best_hdop = (hd>0?hd:best_hdop);
    best_used = su;  best_view = sv;  have_best = true;
  }

  // ¿Cumple objetivo actual? Si sí, aceptamos ya.
  if (have_idx <= target_idx && hd>0) { lat = tlat; lon = tlon; got = true; }

  // envío inmediato (rate-limit 6 s) si mejoró
  if (!got && improved && (millis() - last_refine_telegram_ms) >= 6000UL) {
    unsigned long rem = (start + timeout_ms > millis()) ? (start + timeout_ms - millis()) : 0UL;
    (void)telegramSend(buildRefineText(target_idx, last_sats_used, last_sats_view, last_hdop,
                                       rem, tlat, tlon, true));
    last_refine_telegram_ms = millis();
  }
}

    vTaskDelay(pdMS_TO_TICKS(GNSS_READ_SLEEP_MS));
  }

  bool aborted = s_abortReq;
  s_abortReq = false;

  // Resultado
  String approx_lat, approx_lon; int approx_acc = -1; bool approx_ok = false;

  if (!aborted && got) {
    endWindow("GPS_FIX_SEARCH");
    int tier = classifyTier(last_hdop>0?last_hdop:99.0f, last_sats_used);
    logMsg(LOG_INFO, "GPS", String("FIX OK; usados=") + last_sats_used + " vista=" + last_sats_view +
                      (tier<4? String(" (") + kTiers[tier].name + ")" : " (calidad baja)"));

    respuesta  = "📍 Ubicación: https://maps.google.com/?q=" + lat + "," + lon;
    respuesta += "\n🛰️ Satélites: " + String(last_sats_used) + " usados / " + String(last_sats_view) + " en vista";
    if (last_hdop > 0) respuesta += "\n🎯 HDOP: " + String(last_hdop, 1);
    respuesta += "\n" + String(kTiers[tier].emoji) + " Calidad: *" + String(kTiers[tier].name) + "*";

    if (s_gpsTimer) xTimerStop(s_gpsTimer, 0);
    s_timerMode = false;
    powerOffGPS();
    logMark("GPS_OFF");
    (void)telegramFlushQuick(1200);

  } else if (!aborted) {
    endWindow("GPS_FIX_SEARCH");

    // No cumplió objetivo: usa mejor solución vista, y si no hay, CLBS
    if (have_best) {
      int tier = classifyTier(best_hdop>0?best_hdop:99.0f, best_used);
      logMsg(LOG_WARN, "GPS", String("Timeout; devolviendo mejor FIX visto (") + kTiers[tier].name + ")");
      respuesta  = "📍 Ubicación: https://maps.google.com/?q=" + best_lat + "," + best_lon;
      respuesta += "\n🛰️ Satélites: " + String(best_used) + " usados / " + String(best_view) + " en vista";
      if (best_hdop > 0) respuesta += "\n🎯 HDOP: " + String(best_hdop, 1);
      respuesta += "\n" + String(kTiers[tier].emoji) + " Calidad: *" + String(kTiers[tier].name) + "* (mejor alcanzada)";
      if (!s_timerMode) { powerOffGPS(); logMark("GPS_OFF"); }
      (void)telegramFlushQuick(1200);
      s_fixInProgress = false;
      return true;
    }

    startWindow("GPS_CLBS");
    logMsg(LOG_WARN, "GPS", "Sin FIX; intentando CLBS…");
    approx_ok = getApproxLocationCLBS_ShortLocked(approx_lat, approx_lon, approx_acc, 15000);
    endWindow("GPS_CLBS");

    if (approx_ok) {
      logMsg(LOG_INFO, "GPS", String("CLBS OK; acc≈") + (approx_acc>0?String(approx_acc):"N/A") + " m");
      respuesta  = "📍 Ubicación (aprox.): https://maps.google.com/?q=" + approx_lat + "," + approx_lon;
      if (approx_acc > 0) respuesta += "\n±" + String(approx_acc) + " m";
      respuesta += "\nℹ️ Sin fix GNSS en " + String((timeout_ms+500)/1000) + " s; posición por red móvil.";
      if (!s_timerMode) { powerOffGPS(); logMark("GPS_OFF"); }
      (void)telegramFlushQuick(1200);
      s_fixInProgress = false;
      return true;
    } else {
      logMsg(LOG_WARN, "GPS", "CLBS falló; no se pudo obtener ubicación");
      respuesta = "❌ No he podido obtener fix en " + String((timeout_ms+500)/1000) +
                  " s. Inténtalo al aire libre.";
      if (!s_timerMode) { powerOffGPS(); logMark("GPS_OFF"); }
      (void)telegramFlushQuick(1200);
    }
  } else {
    endWindow("GPS_FIX_SEARCH");
    logMsg(LOG_INFO, "GPS", "Intento de FIX abortado por solicitud");
    powerOffGPS();
    logMark("GPS_OFF");
    (void)telegramFlushQuick(1200);
  }

  s_fixInProgress = false;
  return (!aborted) && (got || approx_ok);
}

void checkGPSStatus(unsigned long /*now*/) {
  // No-op
}
