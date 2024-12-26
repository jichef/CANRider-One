#ifndef OUTPUT_H
#define OUTPUT_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Prototipos de funciones
void initializeOutput(BluetoothSerial &btSerial, Adafruit_ST7735 &tftDisplay);
void logToOutput(const String &message);
void logToOutput_P(const char *message);
void logToOutputln(const String &message);
void displayBatteryState();

#endif
