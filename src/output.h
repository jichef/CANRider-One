#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>
#include <BluetoothSerial.h>


// Prototipos de funciones
void initializeOutput(BluetoothSerial &btSerial);
void logToOutput(const String &message);
void logToOutput_P(const char *message);
void logToOutputln(const String &message);




#endif
