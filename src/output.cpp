#include "output.h"
#include "globals.h"
#include "NotoSans_VariableFont_wdth_wght5pt7b.h"

// Variables internas para manejo de salidas
static BluetoothSerial *serialBT = nullptr;
static Adafruit_ST7735 *tft = nullptr;
static int cursorY = 0; // Posición actual del cursor en la pantalla TFT
#define CURSOR_OFFSET_X 2 // Desplazamiento horizontal
#define CURSOR_OFFSET_Y 2 // Desplazamiento vertical
#define LOG_AREA_HEIGHT (tft->height() * 6 / 8) // Altura para el área de log (6/8 de la pantalla)


// Variable externa para batterylevel
extern int batterylevel;

// Inicializar las salidas
void initializeOutput(BluetoothSerial &btSerial, Adafruit_ST7735 &tftDisplay) {
    serialBT = &btSerial;
    tft = &tftDisplay;

    // Inicializar Bluetooth
    if (!serialBT->begin(btDeviceName)) {
        Serial.println(F("¡Falló la inicialización de Bluetooth!"));
    } else {
        Serial.print(F("Bluetooth inicializado como: "));
        Serial.println(btDeviceName);
    }

    // Inicializar pantalla TFT
    tft->initR(INITR_BLACKTAB);
    tft->setRotation(1);
    tft->fillScreen(ST77XX_BLACK);
    tft->setTextColor(ST77XX_WHITE);
    tft->setFont(&NotoSans_VariableFont_wdth_wght5pt7b);
    tft->setTextSize(1);
    tft->println(F("Iniciando..."));
    Serial.println(F("Pantalla TFT inicializada correctamente."));

}

// Registrar un mensaje en Serial, Bluetooth y la mitad superior de la TFT
void logToOutput(const String &message) {
    Serial.print(message);

    if (serialBT) {
        serialBT->print(message);
    }

    if (tft) {
        tft->setCursor(CURSOR_OFFSET_X, CURSOR_OFFSET_Y + cursorY);
        tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft->print(message);

        cursorY += 10; // Incremento del cursor
        if (cursorY + 10 > LOG_AREA_HEIGHT) { // Si alcanza el límite de 7/8
            tft->fillRect(0, 0, tft->width(), LOG_AREA_HEIGHT, ST77XX_BLACK); // Limpia solo 7/8
            cursorY = CURSOR_OFFSET_Y; // Reinicia el cursor al inicio
        }
    }
}
// Mostrar batterylevel en la mitad inferior
void displayBatteryState() {
    if (tft) {
        // Limpia la parte inferior de la pantalla para mostrar la batería
        tft->fillRect(0, tft->height() - 40, tft->width(), 40, ST77XX_BLACK);  // Limpia la zona de batería

        // Establece el cursor en la parte inferior, pero por encima del área limpiada
        tft->setCursor(CURSOR_OFFSET_X, tft->height() - 18);
        tft->setTextColor(ST77XX_GREEN);
        tft->setTextSize(1);

        // Mostrar el texto concatenado con el valor de batterylevel
        String batteryMessage = "Bateria del vehiculo: " + String(batterylevel) + "%";
        tft->print(batteryMessage);
    }
}




// Registrar un mensaje con salto de línea
void logToOutputln(const String &message) {
    logToOutput(message + "\n");
}
