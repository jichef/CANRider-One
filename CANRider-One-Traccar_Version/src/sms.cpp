#include "sms.h"
#include "output.h"
#include "messages.h"
#include "globals.h"
#include "modem.h"
#include <Arduino.h>

// Obtener últimas coordenadas conocidas
float getLastLat() {
    return lat;
}

float getLastLon() {
    return lon;
}

void sendSMS(TinyGsm &modem, const String &phoneNumber, const String &message) {
    modem.sendSMS(phoneNumber, message);
}

void checkForSMS(TinyGsm &modem) {
    if (!modem.stream.available()) return;

    String line = modem.stream.readStringUntil('\n');
    line.trim();

    if (!line.startsWith("+CMTI:")) return;

    logToOutputln("SMS detectado: " + line);

    int indexStart = line.indexOf(",") + 1;
    if (indexStart <= 0) {
        logToOutputln("Índice inválido en CMTI.");
        return;
    }

    String indexStr = line.substring(indexStart);
    indexStr.trim();
    int msgIndex = indexStr.toInt();

    if (msgIndex <= 0) {
        logToOutputln("Índice de mensaje no válido.");
        return;
    }

    String command = "+CMGR=" + String(msgIndex);
    modem.sendAT(command);
    delay(500);

    String smsResponse = "";
    String partial;
    unsigned long start = millis();
    while (millis() - start < 3000) {  // Esperar hasta 3s máximo
        if (modem.stream.available()) {
            partial = modem.stream.readStringUntil('\n');
            smsResponse += partial + "\n";

            if (partial.indexOf("OK") != -1) {
                break;
            }
        }
    }

    if (smsResponse.length() == 0) {
        logToOutputln("Error al leer contenido del SMS.");
        return;
    }

    logToOutputln("Contenido del SMS:");
    logToOutputln(smsResponse);

    processSMS(modem, smsResponse);
    
    modem.sendAT("+CMGD=" + String(msgIndex));
    logToOutputln("SMS eliminado.");
}



void processSMS(TinyGsm &modem, const String &response) {
    String content = response;
    content.toLowerCase();
    content.trim();

    char senderPhoneNumber[20] = "";
    char *firstQuote = strchr(response.c_str(), '"');
    if (firstQuote) {
        char *secondQuote = strchr(firstQuote + 1, '"');
        if (secondQuote) {
            char *thirdQuote = strchr(secondQuote + 1, '"');
            if (thirdQuote) {
                char *fourthQuote = strchr(thirdQuote + 1, '"');
                if (fourthQuote) {
                    strncpy(senderPhoneNumber, thirdQuote + 1, fourthQuote - thirdQuote - 1);
                    senderPhoneNumber[fourthQuote - thirdQuote - 1] = '\0';
                }
            }
        }
    }

    String sender = String(senderPhoneNumber);
    sender.trim();

    logToOutputln("Comparando remitente con autorizado:");
    logToOutputln("SMS: " + sender);
    logToOutputln("Autorizado: " + String(phoneNumber));

    bool autorizado = (sender == phoneNumber) || (content.indexOf(SMS_KEYWORD_SECURITY) != -1);

    if (autorizado) {
        logToOutputln("✅ Autorización confirmada.");

        if (content.indexOf(SMS_KEYWORD_REBOOT) != -1) {
            logToOutputln("Reboot command detected.");
            sendSMS(modem, sender, "Reinicio en curso...");
            modem.sendAT("+CMGD=1,4");
            delay(2000); // Opcional en FreeRTOS si usas múltiples tareas
            vTaskEndScheduler();             
            ESP.restart();        // Luego reinicia
        }
        else if (content.indexOf(SMS_KEYWORD_GPS) != -1) {
            logToOutputln("GPS request command detected.");
            float lastLat = getLastLat();
            float lastLon = getLastLon();

            float batteryState = (((float)battery - 3.0) / 1.2) * 100.0;
            batteryState = constrain(batteryState, 0, 100);

            if (lastLat != 0.0 && lastLon != 0.0) {
                String link = "https://www.google.com/maps?q=" + String(lastLat, 6) + "," + String(lastLon, 6);
                String gpsResponse = "Ultima ubicacion GPS: " + link +
                                     ". Bateria al " + String(batterylevel, 1) +
                                     "%. El ESP al " + String(batteryState, 1) + "%";
                sendSMS(modem, sender, gpsResponse);
            } else {
                String gpsResponse = "No hay ubicacion GPS valida. Bateria al " + String(batterylevel, 1) +
                                     "%. El ESP al " + String(batteryState, 1) + "%";
                sendSMS(modem, sender, gpsResponse);

            }
            modem.sendAT("+CMGD=1,4");
        }

    } else {
        logToOutputln("⚠️ Número NO autorizado: " + sender);
        sendSMS(modem, sender, "No estás autorizado para usar este sistema.");
    }
}
