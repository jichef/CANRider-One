#ifndef MODEM_H
#define MODEM_H

#include "globals.h"
#include <TinyGsmClient.h>


// Prototipos de funciones
void modemPowerOn();
void modemPowerOff();
void modemRestart();
void initializeModem(TinyGsm &modem);
bool connectToNetwork(TinyGsm &modem, const char *apn, const char *user, const char *pass);
void enableGPS(TinyGsm &modem);
void disableGPS(TinyGsm &modem);
bool getGPSData(TinyGsm &modem, float &lat, float &lon, float &speed, float &alt, float &accuracy);

#endif
