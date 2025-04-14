#include "modem.h"
#include "output.h"
#include "messages.h"
#include "globals.h"

// Encender el módem
 void modemPowerOn() {
    pinMode(PWR_PIN, OUTPUT);     // ← esta línea puede estar usando un valor inválido
    digitalWrite(PWR_PIN, LOW);
    delay(100);
    digitalWrite(PWR_PIN, HIGH);
    delay(1000); // Pulso de encendido
 }


// Apagar el módem
void modemPowerOff() {
    logToOutputln(MODEM_POWER_OFF);
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, LOW);
    delay(1500);
    digitalWrite(PWR_PIN, HIGH);
}

// Reiniciar el módem
void modemRestart() {
    logToOutputln(MODEM_RESTART);
    modemPowerOff();
    delay(1000);
    modemPowerOn();
}

// Inicializar el módem
void initializeModem(TinyGsm &modem) {
    modem.init();
    logToOutputln("→ Reiniciando módem...");
    if (!modem.restart()) {
        logToOutputln(ERROR_MODEM_INIT);
    }

    logToOutputln("→ Desbloqueando SIM (si es necesario)");
    if (strlen(GSM_PIN) > 0 && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }

    logToOutputln("→ Verificando estado de la SIM");
    modem.sendAT("+CPIN?");
    String response = modem.stream.readStringUntil('\n');
    logToOutput(AT_RESPONSE);
    logToOutputln(response);

    if (response.indexOf("READY") != -1 || response.isEmpty()) {
        logToOutputln(SIM_READY_MESSAGE);
    } else {
        logToOutputln(SIM_NOT_READY_MESSAGE);
    }

    logToOutputln("→ Configurando el módem");
    modem.sendAT("+CMGF=1");
    modem.waitResponse();
    modem.sendAT("+CNMI=2,1,0,0,0");
    modem.waitResponse();
    logToOutputln(MODEM_CONFIGURED);

    SIM70xxRegStatus status;
    uint32_t timeout = millis();

    do {
        logToOutputln("→ Verificando señal...");
        int16_t signalQuality = modem.getSignalQuality() * -1;
        status = modem.getRegistrationStatus();

        if (status == REG_DENIED) {
            logToOutputln(SIM_DENIED);
            return;
        } else {
            logToOutput(SIGNAL_LEVEL);
            logToOutputln(String(signalQuality));
        }

        if (millis() - timeout > 120000) {
            if (signalQuality == 99) {
                logToOutputln(SIM_NO_COVERAGE);
                reboot();
                return;
            }
            timeout = millis();
        }

        delay(800);
    } while (status != REG_OK_HOME && status != REG_OK_ROAMING);

    logToOutputln("→ Obteniendo IMEI...");
    String imei = modem.getIMEI();
    logToOutput(MODEM_IMEI);
    logToOutputln(imei);

    logToOutputln("→ Activando contexto de red...");
    modem.sendAT("+CNACT=1");
    modem.waitResponse();

    modem.sendAT("+CNACT?");
    if (modem.waitResponse("+CNACT: ") == 1) {
        modem.stream.read();
        modem.stream.read();
        String res = modem.stream.readStringUntil('\n');
        res.replace("\"", "");
        res.replace("\r", "");
        res.replace("\n", "");
        modem.waitResponse();
    }

    logToOutputln("→ Obteniendo parámetros de red...");
    modem.sendAT("+CPSI?");
    if (modem.waitResponse("+CPSI: ") == 1) {
        String res = modem.stream.readStringUntil('\n');
        res.replace("\r", "");
        res.replace("\n", "");
        modem.waitResponse();
        logToOutput(NETWORK_PARAMS);
        logToOutputln(res);
    }

    logToOutputln("→ MODEM_INIT_SUCCESS");
}


// Activar GPS
void enableGPS(TinyGsm &modem) {
    logToOutputln(GPS_START);
    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        logToOutputln(GPS_FAIL);
    }
    modem.enableGPS();
    logToOutputln(GPS_ON);
}

// Desactivar GPS
void disableGPS(TinyGsm &modem) {
    logToOutputln(GPS_STOP);
    modem.sendAT("+SGPIO=0,4,1,0");
    if (modem.waitResponse(10000L) != 1) {
        logToOutputln(GPS_FAIL);
    }
    modem.disableGPS();
    logToOutputln(GPS_OFF);
}

// Obtener datos de GPS
bool getGPSData(TinyGsm &modem, float &lat, float &lon, float &speed, float &alt, float &accuracy) {
    logToOutputln(GPS_READING);
    int vsat, usat, year, month, day, hour, min, sec;

    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
        logToOutput(GPS_LAT);
        logToOutputln(String(lat, 6));
        logToOutput(GPS_LON);
        logToOutputln(String(lon, 6));
        logToOutput(GPS_SPEED);
        logToOutputln(String(speed, 2));
        logToOutput(GPS_ALT);
        logToOutputln(String(alt, 2));
        logToOutput(GPS_ACCURACY);
        logToOutputln(String(accuracy, 2));
        logToOutput(GPS_VSAT);
        logToOutputln(String(vsat));
        logToOutput(GPS_USAT);
        logToOutputln(String(usat));
        return true;
    } else {
        logToOutputln(GPS_ERROR);
        return false;
    }
}

// Conectar a la red GPRS
bool connectToNetwork(TinyGsm &modem, const char *apn, const char *user, const char *pass) {
    logToOutput(GPRS_APN);
    logToOutputln(apn);
    if (!modem.gprsConnect(apn, user, pass)) {
        logToOutputln(GPRS_FAIL);
        return false;
    }
    logToOutputln(GPRS_SUCCESS);
    logToOutput(IP_ASSIGNED);
    logToOutputln(modem.getLocalIP());
    return true;
}
