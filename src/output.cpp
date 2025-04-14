#include "output.h"
#include "globals.h"

// Variables internas para manejo de salidas
static BluetoothSerial *serialBT = nullptr;

void initializeOutput(BluetoothSerial &btSerial) {
    serialBT = &btSerial;
    // Inicializar Bluetooth
    if (!serialBT->begin(btDeviceName)) {
        Serial.println(F("¡Falló la inicialización de Bluetooth!"));
    } else {
        Serial.print(F("Bluetooth inicializado como: "));
        Serial.println(btDeviceName);
    }
}

// Registrar un mensaje en Serial, Bluetooth y la mitad superior de la TFT
void logToOutput(const String &message) {
    Serial.print(message);

    if (serialBT) {
        serialBT->print(message);
    }

}

// Registrar un mensaje con salto de línea
void logToOutputln(const String &message) {
    logToOutput(message + "\n");
}
