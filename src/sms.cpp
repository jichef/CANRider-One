#include "sms.h"
#include "output.h"
#include "messages.h"
#include "globals.h"
#include "modem.h"
#include <Arduino.h>

// Definir palabra clave de GPS
#define SMS_KEYWORD_GPS "gps"

// Variables globales para almacenar la última ubicación

float getLastLat() {
    // Deberías tener una forma de obtener las últimas coordenadas lat
    return lat;  // Usa la variable `lat` que tienes en el código de `taskGPSTraccar`
}

float getLastLon() {
    // Deberías tener una forma de obtener las últimas coordenadas lon
    return lon;  // Usa la variable `lon` que tienes en el código de `taskGPSTraccar`
}

void sendSMS(TinyGsm &modem, const String &phoneNumber, const String &message) {
    modem.sendSMS(phoneNumber, message); // Lógica para enviar SMS
}

// Verificar si hay SMS nuevos y procesarlos
void checkForSMS(TinyGsm &modem) {
  readSMS(modem);
}

void readSMS(TinyGsm &modem) {
    logToOutputln("Reading SMS...");
    modem.sendAT("+CMGL=\"REC UNREAD\"");  // Obtener todos los mensajes no leídos
    delay(1000);  // Esperar un poco para que el módem procese el comando

    // Leer la respuesta del módem
    String response = modem.stream.readString();
    logToOutputln("SMS List: ");
    logToOutputln(response);

    // Verificar si hubo un error
    if (response.indexOf("ERROR") != -1) {
        logToOutputln("Error while reading SMS. Retrying...");
        modem.sendAT("+CMGL=\"REC UNREAD\"");  // Reintentar la lectura
        delay(2000);  // Esperar un poco más antes de leer
        response = modem.stream.readString(); // Volver a leer la respuesta
        logToOutputln("Retry SMS List: ");
        logToOutputln(response);
    }

    // Convertir el texto recibido a minúsculas para comparación insensible a mayúsculas
    response.toLowerCase();  // Convertir todo el texto a minúsculas
    // Extraer el número de teléfono del remitente
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
                    senderPhoneNumber[fourthQuote - thirdQuote - 1] = '\0'; // Asegurar terminación de cadena
                }
            }
        }
    }
    logToOutputln("Sender phone number detected: " + String(senderPhoneNumber));

    // Verificar si contiene la palabra "reboot"
    if (response.indexOf(SMS_KEYWORD_REBOOT) != -1) {
        logToOutputln("Reboot command detected.");
        // Comparar con el número de teléfono almacenado
        if (String(senderPhoneNumber) == phoneNumber) {
            logToOutputln("Message from the correct phone number. Restarting...");

            // Enviar un SMS de confirmación antes del reinicio
            String confirmationMessage = "Reinicio del sistema en curso...";
            sendSMS(modem, senderPhoneNumber, confirmationMessage);  // Enviar SMS de confirmación
            logToOutputln("SMS sent to " + String(senderPhoneNumber));

            // Asegúrate de dar tiempo suficiente para enviar el SMS
            delay(2000);  // Esperar 2 segundos antes de reiniciar

            // Reiniciar el ESP32
            logToOutputln("Rebooting...");
            ESP.restart();  // Reiniciar el ESP32
        }
    }

    // Verificar si contiene la palabra "gps"
    if (response.indexOf(SMS_KEYWORD_GPS) != -1) {
        logToOutputln("GPS request command detected.");

        // Obtener la última ubicación GPS (lat y lon deberían ser proporcionados por la función sendToTraccar)
        float lat = getLastLat(); // Asegúrate de tener una función que devuelva lat
        float lon = getLastLon(); // Asegúrate de tener una función que devuelva lon

        // Verificar si la ubicación GPS es válida
        if (lat != 0.0 && lon != 0.0) {
            // Generar el enlace de Google Maps con la ubicación
            String gpsLink = "https://www.google.com/maps?q=" + String(lat, 6) + "," + String(lon, 6);

            // Enviar la respuesta con la URL de Google Maps
            String gpsResponse = "Última ubicación GPS: " + gpsLink;
            sendSMS(modem, senderPhoneNumber, gpsResponse);  // Enviar el enlace con la ubicación
            logToOutputln("GPS link sent to " + String(senderPhoneNumber));
        } else {
            // Si no hay ubicación disponible, enviar un mensaje de error
            String gpsResponse = "No se ha obtenido ubicación GPS válida.";
            sendSMS(modem, senderPhoneNumber, gpsResponse);
            logToOutputln("No GPS data available. Sent error message to " + String(senderPhoneNumber));
        }

        // Eliminar el mensaje después de procesarlo
        modem.sendAT("+CMGD=0");  // Eliminar el primer mensaje leído
        logToOutputln("Message deleted.");
    }
}

void deleteAllSMS(TinyGsm &modem) {
    logToOutputln("Deleting all SMS...");

    // Verificar la memoria de SMS
    modem.sendAT("+CPMS?");
    String response = modem.stream.readString();
    logToOutputln("Memory status: ");
    logToOutputln(response);  // Mostrar la memoria utilizada

    // Leer todos los SMS no leídos primero
    modem.sendAT("+CMGL=\"ALL\"");  // Get all unread SMS 
    String listResponse = modem.stream.readString();
    logToOutputln("List of unread messages:");
    logToOutputln(listResponse);

    // Contar cuántos mensajes no leídos están presentes en la respuesta
    int messageCount = 0;
    int index = 0;
    while ((index = listResponse.indexOf("+CMGL:", index)) != -1) {
        messageCount++;
        index += 6;  // Avanzar después de cada "+CMGL:"
    }

    // Si hay mensajes no leídos, eliminar uno por uno
    if (messageCount > 0) {
        for (int i = 1; i <= messageCount; i++) {
            String deleteCommand = "+CMGD=" + String(i);  // Eliminar mensaje por índice
            modem.sendAT(deleteCommand);  // Enviar el comando para eliminar el mensaje
            delay(1000);  // Esperar que el módem procese el comando
            String deleteResponse = modem.stream.readString();
            logToOutputln("Delete response for message " + String(i) + ": " + deleteResponse);
        }

        logToOutputln("All unread messages deleted.");
    } else {
        logToOutputln("No unread messages found to delete.");
    }
}
