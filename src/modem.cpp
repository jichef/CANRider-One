// modem.cpp
#include <Arduino.h>
#include "config_user.h"   // APN, GPRS_USER, GPRS_PASS, BOT_TOKEN, UART_BAUD, MODEM_RX, MODEM_TX, PWR_PIN
#include "config.h"
#include "modem.h"

#include <TinyGsmClientSIM7000SSL.h>
#include <UniversalTelegramBot.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
}

// ===================== Shim de logging local =====================
#ifndef LOG_DEBUG
#define LOG_DEBUG 0
#endif
#ifndef LOG_INFO
#define LOG_INFO  1
#endif
#ifndef LOG_WARN
#define LOG_WARN  2
#endif

static inline void logMsg(int lvl, const char* tag, const String& msg) {
  Serial.print("[");
  if (lvl == LOG_DEBUG) Serial.print("DEBUG");
  else if (lvl == LOG_INFO) Serial.print("INFO");
  else if (lvl == LOG_WARN) Serial.print("WARN");
  else Serial.print("LOG");
  Serial.print("] ");
  Serial.print(tag);
  Serial.print(" — ");
  Serial.println(msg);
}

// ===================== Instancias únicas =====================
TinyGsmSim7000SSL modem(Serial1);
TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient(modem);
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

bool gprsConnected = false;

// ===================== TLS config =====================
#ifndef TG_TLS_TIMEOUT_MS
#define TG_TLS_TIMEOUT_MS 20000UL
#endif
static int s_tlsVersion = 3;  // 3=TLS1.2, fallback a 4 si probe falla

// ===================== Helpers privados =====================

// Pulso de arranque al SIM7000 (PWRKEY)
static void modemPowerPulse() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(100);
  digitalWrite(PWR_PIN, LOW);     // PWRKEY a LOW ~1.5s
  delay(1500);
  digitalWrite(PWR_PIN, HIGH);
  delay(3000);
}

// Espera respuesta AT “OK” hasta ms
static bool waitForAT(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    modem.sendAT();
    if (modem.waitResponse(500, "OK") == 1) return true;
    delay(200);
  }
  return false;
}

// PDP IPv4 + DNS conocidos
static void applyDnsPdpIPv4(const char* apn) {
  modem.sendAT("+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"");        modem.waitResponse(2000);
  String cmd = String("+CGDCONT=1,\"IP\",\"") + apn + "\""; // IPv4 (IP)
  modem.sendAT(cmd);                                        modem.waitResponse(2000);
}

// ===== TLS (SIM7000) =====
static void tlsPrepare() {
  logMsg(LOG_INFO, "TLS", "Preparando perfil TLS (seclevel=0, sni=1, ignorertctime=1, ver=TLS1.2)");
  secureClient.stop();
  secureClient.setTimeout(TG_TLS_TIMEOUT_MS);

  unsigned long t0 = millis();
  modem.sendAT("+CSSLCFG=\"seclevel\",1,0");        modem.waitResponse(800);  // no verify
  modem.sendAT("+CSSLCFG=\"sni\",1,1");             modem.waitResponse(400);  // SNI on
  modem.sendAT("+CSSLCFG=\"ignorertctime\",1,1");   modem.waitResponse(400);  // ignora RTC
  modem.sendAT("+CSSLCFG=\"sslversion\",1,3");      modem.waitResponse(400);  // TLS1.2
  modem.sendAT("+CSSLCFG=\"negotiatetime\",1,30");  modem.waitResponse(400);  // <— 30 s máx handshake (si soporta)
  unsigned long dt = millis() - t0;

  s_tlsVersion = 3;
  logMsg(LOG_INFO, "TLS", String("CSSLCFG aplicado en ") + dt + " ms (ver=3)");
}

static inline void tlsWarmDelay() {
  logMsg(LOG_INFO, "TLS", "Warm delay 1500 ms antes del primer handshake…");
  vTaskDelay(pdMS_TO_TICKS(1500));
}

// Prueba de conexión TLS (abre/cierra). Mide tiempo y hace fallback de versión.
static bool tlsProbe(const char* host, uint16_t port = 443) {
  logMsg(LOG_INFO, "TG_HTTP", String("Probe TLS a ") + host + ":" + port + " (ver=" + s_tlsVersion + ")…");
  unsigned long t0 = millis();
  bool ok = secureClient.connect(host, port);
  unsigned long dt = millis() - t0;

  if (!ok) {
    logMsg(LOG_WARN, "TG_HTTP", String("connect FAIL ver=") + s_tlsVersion + " dt=" + dt + " ms; probando ver=4");
    secureClient.stop();
    modem.sendAT("+CSSLCFG=\"sslversion\",1,4"); modem.waitResponse(500);
    s_tlsVersion = 4;
    t0 = millis();
    ok = secureClient.connect(host, port);
    dt = millis() - t0;
  }

  secureClient.stop();
  logMsg(ok ? LOG_INFO : LOG_WARN, "TG_HTTP",
         String(ok ? "probe OK" : "probe FAIL") + " dt=" + dt + " ms (ver=" + s_tlsVersion + ")");
  return ok;
}

// Diagnóstico DNS simple
static void dnsDiagTelegram() {
  logMsg(LOG_INFO, "DNS", "CDNSGIP api.telegram.org…");
  modem.sendAT("+CDNSGIP=\"api.telegram.org\"");
  modem.waitResponse(4000); // mostrará +CDNSGIP: … si lo soporta
}

// Warm-up 1: un ping IPv4 rápido para abrir camino/NAT
static void netWarmupPing() {
  logMsg(LOG_INFO, "PING", "SNPING4 1.1.1.1 x1, 1500 ms…");
  modem.sendAT("+SNPING4=\"1.1.1.1\",1,32,1500");
  modem.waitResponse(3000); // no evaluamos resultado; solo “calentar”
}

// Warm-up 2: abrir y cerrar un TCP a puerto 80 (sin TLS) para asentar NAT
static void netWarmupTcp() {
  logMsg(LOG_INFO, "TCP", "CIPOPEN TCP connectivitycheck.gstatic.com:80 (warm-up) …");
  modem.sendAT("+CIPOPEN=0,\"TCP\",\"connectivitycheck.gstatic.com\",80");
  modem.waitResponse(12000); // OK o ERROR según firmware/estado
  modem.sendAT("+CIPCLOSE=0");
  modem.waitResponse(3000);
  logMsg(LOG_INFO, "TCP", "warm-up TCP terminado");
}

// ===================== API expuesta =====================

void initModem() {
  Serial1.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modemPowerPulse();

  Serial.println("→ Inicializando módem...");
  modem.init();

  if (!waitForAT(5000)) {
    Serial.println("⚠️ Sin respuesta AT inicial. Reintentando power-on...");
    modemPowerPulse();
    if (!waitForAT(5000)) {
      Serial.println("❌ Módem no responde a AT");
    }
  }
}

/**
 * Lee hora del módem con robustez:
 *  - Drena buffer previamente
 *  - Espera hasta 2500 ms una línea +CCLK
 *  - Valida HH:MM y REINTENTA si es 00:00 (transitorio)
 */
static bool readCCLKOnce(int &hour, int &minute, uint16_t wait_ms) {
  while (modem.stream.available()) modem.stream.read();

  modem.sendAT("+CCLK?");
  const unsigned long t0 = millis();
  String line;

  while (millis() - t0 < wait_ms) {
    delay(10);
    while (modem.stream.available()) {
      line = modem.stream.readStringUntil('\n');
      line.trim();
      if (line.startsWith("+CCLK:")) {
        int q1 = line.indexOf('"');
        int q2 = line.lastIndexOf('"');
        if (q1 < 0 || q2 <= q1) return false;

        int comma = line.indexOf(',', q1);
        if (comma < 0 || comma + 6 >= q2) return false;

        String timePart = line.substring(comma + 1, q2);
        if (timePart.length() < 5) return false;

        int h = timePart.substring(0, 2).toInt();
        int m = timePart.substring(3, 5).toInt();
        if (h < 0 || h > 23 || m < 0 || m > 59) return false;

        hour = h; minute = m;
        return true;
      }
    }
  }
  return false;
}

bool getTimeFromModem(int &hour, int &minute) {
  if (!readCCLKOnce(hour, minute, 2500)) return false;

  if (hour == 0 && minute == 0) {
    delay(200);
    int h2=0, m2=0;
    if (readCCLKOnce(h2, m2, 1200)) {
      if (!(h2 == 0 && m2 == 0)) { hour = h2; minute = m2; }
    }
    if (hour == 0 && minute == 0) return false;
  }
  return true;
}

// --- Red / modo radio (DIGI ejemplo) ---
void setNetworkModeDIGI() {
  modem.sendAT("+CNMP=51");  modem.waitResponse(2000); // AUTO
  modem.sendAT("+CMNB=1");   modem.waitResponse(2000); // Cat-M
}

// Configuración PDP/DNS y parámetros TLS base
void applyDnsPdpAndTls(const char* apn) {
  applyDnsPdpIPv4(apn);
  modem.sendAT("+CSSLCFG=\"sslversion\",1,3");     modem.waitResponse(1000);
  modem.sendAT("+CSSLCFG=\"seclevel\",1,0");       modem.waitResponse(800);
  modem.sendAT("+CSSLCFG=\"ignorertctime\",1,1");  modem.waitResponse(800);
  modem.sendAT("+CSSLCFG=\"sni\",1,1");            modem.waitResponse(800);
}

/**
 * NTP + verificación de CCLK (no 00:00).
 */
bool syncModemTimeNTP() {
  modem.sendAT("+CNTP=\"pool.ntp.org\",0");
  if (modem.waitResponse(4000) != 1) return false;

  modem.sendAT("+CNTP");
  if (modem.waitResponse(15000, "OK") != 1) return false;

  int h=0, m=0;
  bool ok = getTimeFromModem(h, m);
  if (ok) Serial.println("🕰️ Hora del módem sincronizada por NTP");
  return ok;
}

bool connectGPRS() {
  if (gprsConnected) return true;

  Serial.println("📶 Intentando GPRS...");

  if (!waitForAT(3000)) {
    modemPowerPulse();
    if (!waitForAT(5000)) {
      Serial.println("❌ Módem sin AT");
      return false;
    }
  }

  modem.sendAT("+IPR=115200"); modem.waitResponse(2000);
  modem.sendAT("&W");          modem.waitResponse(2000);

  // SIM PIN (opcional)
  modem.sendAT("+CPIN?");
  if (modem.waitResponse(2000, "+CPIN:") == 1) {
    String line = modem.stream.readStringUntil('\n'); line.trim();
    if (line.indexOf("SIM PIN") >= 0) {
      #ifdef SIM_PIN_CODE
      if (strlen(SIM_PIN_CODE)) {
        Serial.println("🔓 Introduciendo PIN...");
        modem.simUnlock(SIM_PIN_CODE);
        delay(2000);
      } else {
        Serial.println("❌ La SIM pide PIN y no hay SIM_PIN_CODE");
        return false;
      }
      #endif
    }
  }

  setNetworkModeDIGI();
  applyDnsPdpAndTls(APN);

  Serial.println("⏳ Esperando red...");
  if (!modem.waitForNetwork(180000L)) {
    Serial.println("❌ No hay red (registrado)");
    return false;
  }

  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    Serial.println("❌ GPRS falló");
    return false;
  }

  Serial.println("✅ GPRS conectado");
  gprsConnected = true;

  // NTP (opcional)
  if (!syncModemTimeNTP()) {
    Serial.println("ℹ️ NTP no disponible ahora (no crítico con seclevel=0)");
  }

  // ——— Warm-up de red antes del primer TLS ———
  netWarmupPing();
  netWarmupTcp();

  // Secuencia TLS con logs de progreso
  logMsg(LOG_INFO, "TLS", "Iniciando preparación y prueba de TLS…");
  tlsPrepare();
  tlsWarmDelay();

  // Diagnóstico DNS + probe TLS
  dnsDiagTelegram();
  logMsg(LOG_INFO, "TG_HTTP", "Resolviendo/Probando api.telegram.org:443…");
  bool tlsok = tlsProbe("api.telegram.org", 443);
  if (!tlsok) {
    logMsg(LOG_WARN, "TG_HTTP", "Probe TLS falló (puede ser DNS/APN/cobertura). Se continuará y Telegram reintentará.");
  }

  return true;
}
