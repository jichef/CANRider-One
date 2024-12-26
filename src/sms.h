#ifndef SMS_H
#define SMS_H

#include "globals.h" // Incluye las definiciones globales, como TINY_GSM_MODEM_SIM7000
#include <TinyGsmClient.h>

// Prototipos de funciones para manejar SMS
void sendSMS(TinyGsm &modem, const String &phoneNumber, const String &message);
void checkForSMS(TinyGsm &modem);  // Modificado para aceptar 'modem' como argumento
void readSMS(TinyGsm &modem);      // Modificado para aceptar 'modem' como argumento
void deleteAllSMS(TinyGsm &modem); // Modificado para aceptar 'modem' como argumento
float getLastLat();  // Declarar la función para obtener la última latitud
float getLastLon();  // Declarar la función para obtener la última longitud


#endif
