#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include "globals.h"
#include "output.h"
#include "modem.h"

// Calcula si estamos en horario de verano en España
bool isSpanishSummerTime(int year, int month, int day) {
  int lastSundayMarch = 31 - (5 * year / 4 + 4) % 7;
  int lastSundayOct   = 31 - (5 * year / 4 + 1) % 7;

  if ((month > 3 && month < 10) ||
      (month == 3 && day >= lastSundayMarch) ||
      (month == 10 && day < lastSundayOct)) {
    return true;
  }

  return false;
}

void syncTimeFromHTTPHeader(TinyGsm &modem) {
  logToOutputln("→ Obteniendo hora desde cabecera HTTP...");

  TinyGsmClient client(modem);
  HttpClient http(client, "example.com", 80);
  http.setTimeout(10000);

  http.get("/");

  int statusCode = http.responseStatusCode();
  if (statusCode <= 0) {
    logToOutput("× Error HTTP: ");
    logToOutputln(String(statusCode));
    return;
  }

  // Leer cabeceras hasta encontrar Date:
  String dateHeader;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Date: ")) {
      dateHeader = line;
      break;
    }
    if (line == "\r") break;  // fin de cabeceras
  }

  if (dateHeader.length() < 29) {
    logToOutputln("× Cabecera 'Date' no encontrada o incompleta");
    return;
  }

  logToOutput("Cabecera Date: ");
  logToOutputln(dateHeader);

  // Ejemplo: "Date: Thu, 18 Apr 2025 14:59:03 GMT"
  int hour = dateHeader.substring(23, 25).toInt();
  int min  = dateHeader.substring(26, 28).toInt();
  int day  = dateHeader.substring(11, 13).toInt();

  String monthStr = dateHeader.substring(14, 17);
  int month = 0;

  if      (monthStr == "Jan") month = 1;
  else if (monthStr == "Feb") month = 2;
  else if (monthStr == "Mar") month = 3;
  else if (monthStr == "Apr") month = 4;
  else if (monthStr == "May") month = 5;
  else if (monthStr == "Jun") month = 6;
  else if (monthStr == "Jul") month = 7;
  else if (monthStr == "Aug") month = 8;
  else if (monthStr == "Sep") month = 9;
  else if (monthStr == "Oct") month = 10;
  else if (monthStr == "Nov") month = 11;
  else if (monthStr == "Dec") month = 12;

  int year = dateHeader.substring(18, 22).toInt();

  int offset = isSpanishSummerTime(year, month, day) ? 2 : 1;

  localHour = (hour + offset) % 24;
  minute = min;

  logToOutput("✔ Hora local (España): ");
  if (localHour < 10) logToOutput("0");
  logToOutput(String(localHour));
  logToOutput(":");
  if (minute < 10) logToOutput("0");
  logToOutputln(String(minute));

  http.stop();
}
