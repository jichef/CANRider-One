#include <Arduino.h>
#include "config_user.h"   // APN, GPRS_USER, GPRS_PASS, BOT_TOKEN, pines y baud
#include "modem.h"
#include "config.h"        // ← añade

// Instancias únicas
TinyGsmSim7000SSL modem(Serial1);
TinyGsmSim7000SSL::GsmClientSecureSIM7000SSL secureClient(modem);
UniversalTelegramBot bot(BOT_TOKEN, secureClient);



// ----- Helpers privados -----
static void modemPowerPulse() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(100);
  digitalWrite(PWR_PIN, LOW);    // pulso PWRKEY ~1.5s
  delay(1500);
  digitalWrite(PWR_PIN, HIGH);
  delay(3000);
}

static bool waitForAT(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    modem.sendAT();
    if (modem.waitResponse(500, "OK") == 1) return true;
    delay(200);
  }
  return false;
}

// ----- API expuesta -----
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

bool getTimeFromModem(int &hour, int &minute) {
  // Drenar basura previa
  while (modem.stream.available()) modem.stream.read();

  modem.sendAT("+CCLK?");
  unsigned long t0 = millis();
  String line;

  // Esperar a que llegue una línea con +CCLK
  while (millis() - t0 < 1500) {
    if (modem.stream.available()) {
      line = modem.stream.readStringUntil('\n');
      line.trim();
      if (line.startsWith("+CCLK:")) break;
    }
  }
  if (!line.startsWith("+CCLK:")) return false;

  // Ejemplo: +CCLK: "25/08/09,00:53:21+08"
  int q1 = line.indexOf('"'), q2 = line.lastIndexOf('"');
  if (q1 < 0 || q2 <= q1) return false;

  String timePart = line.substring(line.indexOf(',') + 1, q2);
  if (timePart.length() < 5) return false;

  int h = timePart.substring(0, 2).toInt();
  int m = timePart.substring(3, 5).toInt();

  // Validación fuerte
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  if (h == 0 && m == 0) return false;  // evita “00:00” espurio

  hour = h;
  minute = m;
  return true;
}


// --- Red / TLS (igual a tu código que funcionaba) ---
void setNetworkModeDIGI() {
  modem.sendAT("+CNMP=51");  modem.waitResponse(2000); // AUTO
  modem.sendAT("+CMNB=1");   modem.waitResponse(2000); // Cat-M (si cae a 2G, se ignora)
}

void applyDnsPdpAndTls(const char* apn) {
  modem.sendAT("+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"");  modem.waitResponse(2000);
  String cmd = String("+CGDCONT=1,\"IP\",\"") + apn + "\"";
  modem.sendAT(cmd);                                  modem.waitResponse(2000);

  modem.sendAT("+CSSLCFG=\"sslversion\",1,3");        modem.waitResponse(2000); // TLS1.2
  modem.sendAT("+CSSLCFG=\"authmode\",1,0");          modem.waitResponse(2000); // validar servidor
  modem.sendAT("+CSSLCFG=\"ignorertctime\",1,0");     modem.waitResponse(2000); // exigir hora válida
  modem.sendAT("+CSSLCFG=\"sni\",1,\"api.telegram.org\""); modem.waitResponse(2000);
  // Si tu red falla por CA, como prueba rápida: authmode 0 (no validar)
  // modem.sendAT("+CSSLCFG=\"authmode\",1,0");        modem.waitResponse(2000);
}

bool syncModemTimeNTP() {
  modem.sendAT("+CNTP=\"pool.ntp.org\",0");           if (modem.waitResponse(4000) != 1) return false;
  modem.sendAT("+CNTP");                              if (modem.waitResponse(15000, "OK") != 1) return false;
  modem.sendAT("+CCLK?");                             return modem.waitResponse(2000, "+CCLK:") == 1;
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

  // NTP antes de HTTPS
  if (syncModemTimeNTP()) {
    Serial.println("🕰️ Hora del módem sincronizada por NTP");
  } else {
    Serial.println("⚠️ No se pudo sincronizar la hora por NTP (TLS estricto puede fallar)");
  }

  return true;
}
extern TinyGsmSim7000SSL modem;
extern SemaphoreHandle_t modemMutex;
extern bool gprsConnected;

static inline bool modemLock(uint32_t ms = 7000) {
  return !modemMutex || xSemaphoreTake(modemMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
static inline void modemUnlock() {
  if (modemMutex) xSemaphoreGive(modemMutex);
}

static void diagRadio_nolock() {
  Serial.println(F("=== DIAG RADIO ==="));
  auto dump = [](const char* cmd) {
    modem.sendAT(cmd);
    String data;
    modem.waitResponse(7000, data);   // <- firma compatible
    Serial.print(cmd);
    Serial.print(F(" -> "));
    Serial.println(data);
  };
  dump("+CPIN?");
  dump("+CSQ");
  dump("+COPS?");
  dump("+CEREG?");
  dump("+CGREG?");
  dump("+CPSI?");
  Serial.println(F("=== DIAG FIN ==="));
}


static void setAutoModes_nolock() {
  modem.sendAT("+COPS=0");  modem.waitResponse(10000);
  modem.sendAT("+CNMP=2");  modem.waitResponse(10000);   // auto
  modem.sendAT("+CMNB=3");  modem.waitResponse(10000);   // Cat-M + NB-IoT
  modem.sendAT("+CPSMS=0"); modem.waitResponse(5000);    // sin PSM
  modem.sendAT("+CEDRXS=0");modem.waitResponse(5000);    // sin eDRX
}

static void bounceRadio_nolock() {
  modem.sendAT("+CFUN=0"); modem.waitResponse(10000);
  delay(1500);
  modem.sendAT("+CFUN=1"); modem.waitResponse(10000);
  delay(3000);
}

static bool waitRegistered_nolock(uint32_t ms) {
  uint32_t t0 = millis();
  modem.sendAT("+CEREG=2"); modem.waitResponse(5000);
  while (millis() - t0 < ms) {
    if (modem.isNetworkConnected()) return true;
    delay(1000);
  }
  return false;
}

// ——— Intento robusto de adjuntar datos
bool ensureDataAttached() {
  bool ok = false;
  if (!modemLock()) return false;
  setAutoModes_nolock();
  ok = waitRegistered_nolock(30000);
  if (!ok) {
    bounceRadio_nolock();
    modem.sendAT("+CNMP=2"); modem.waitResponse(10000);
    modem.sendAT("+CMNB=1"); modem.waitResponse(10000); // Cat-M sólo
    ok = waitRegistered_nolock(30000);
  }
  if (!ok) {
    bounceRadio_nolock();
    modem.sendAT("+CNMP=2"); modem.waitResponse(10000);
    modem.sendAT("+CMNB=2"); modem.waitResponse(10000); // NB-IoT sólo
    ok = waitRegistered_nolock(30000);
  }
  // Adjuntar datos
  if (ok && !modem.isGprsConnected()) {
    if (!modem.gprsConnect(APN, APN_USER, APN_PASS)) {
      // Reintento tras CFUN toggle
      bounceRadio_nolock();
      ok = waitRegistered_nolock(20000) && modem.gprsConnect(APN, APN_USER, APN_PASS);
    }
  }
  if (!ok) diagRadio_nolock();
  modemUnlock();
  return ok && modem.isGprsConnected();
}

// ——— Task de vigilancia de red
void taskNetWatchdog(void*) {
  const uint32_t OK_SLEEP_MS      = 60000;    // pausa normal si todo va bien
  const uint32_t BUSY_BACKOFF_MS  = 15000;    // si el módem está ocupado
  const uint32_t TRYLOCK_MS       = 25;       // intento de lock muy corto
  const uint32_t ALIVE_EVERY_MS   = 300000;   // verificación real cada 5 min

  uint32_t fails = 0;
  uint32_t lastAliveCheck = 0;

  for (;;) {
    // Si creemos que hay datos, no toquemos el módem en cada vuelta
    if (gprsConnected && (millis() - lastAliveCheck) < ALIVE_EVERY_MS) {
      vTaskDelay(pdMS_TO_TICKS(OK_SLEEP_MS));
      continue;
    }

    // Intento de lock corto: si está ocupado, NO contar fallo ni insistir
    if (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(TRYLOCK_MS)) != pdTRUE) {
      Serial.println(F("📡 NET WD — módem ocupado; reintento en 15s"));
      vTaskDelay(pdMS_TO_TICKS(BUSY_BACKOFF_MS));
      continue;
    }

    // Con lock tomado: chequeo rápido real
    bool attached = false;
    if (modemMutex) {
      attached = modem.isGprsConnected();
      xSemaphoreGive(modemMutex);
    } else {
      attached = modem.isGprsConnected(); // por compatibilidad si no hay mutex
    }

    if (!attached) {
      Serial.println(F("📡 NET WD — asegurando datos..."));
      bool ok = ensureDataAttached();   // esta función debe usar su propio lock

      // Verificación final corta (con lock) si es posible
      if (modemMutex && xSemaphoreTake(modemMutex, pdMS_TO_TICKS(TRYLOCK_MS)) == pdTRUE) {
        gprsConnected = ok && modem.isGprsConnected();
        xSemaphoreGive(modemMutex);
      } else {
        gprsConnected = ok;  // mejor estimación si no pudimos verificar
      }
    } else {
      gprsConnected = true;
    }

    if (!gprsConnected) {
      fails++;
      uint32_t waitMs = (fails >= 20) ? 300000UL : (15000UL * fails); // 15 s → máx 5 min
      Serial.printf("⏳ NET WD — fallo %lu, reintento en %lus\n",
                    (unsigned long)fails, waitMs/1000);
      vTaskDelay(pdMS_TO_TICKS(waitMs));
    } else {
      fails = 0;
      lastAliveCheck = millis();
      Serial.println(F("✅ NET WD — datos OK"));
      vTaskDelay(pdMS_TO_TICKS(OK_SLEEP_MS));
    }
  }
}

