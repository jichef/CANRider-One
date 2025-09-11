#include "logs.h"
#include <SD.h>
#include <time.h>

#include <SD.h>

void initSD() {
  if (!SD.begin()) {
    Serial.println("❌ No se pudo montar la SD");
  } else {
    Serial.println("✅ SD montada");
  }
}

void logHora(const String &hora) {
  File logFile = SD.open("/logs.txt", FILE_APPEND);
  if (logFile) {
    logFile.print("{\"hora_sistema\":\"");
    logFile.print(hora);
    logFile.println("\"},");
    logFile.close();
  }
}

// Función auxiliar para obtener la hora formateada (hh:mm:ss)
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buffer[20];
  sprintf(buffer, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  return String(buffer);
}

void logEvento(const String &evento) {
  File logFile = SD.open("/logs.txt", FILE_APPEND);
  if (logFile) {
    logFile.print("{\"evento\":\"");
    logFile.print(evento);
    logFile.print("\",\"hora\":\"");
    logFile.print(getFormattedTime());
    logFile.println("\"},");
    logFile.close();
  }
}

void logUbicacion(const String &ubicacion, const String &accion) {
  File logFile = SD.open("/logs.txt", FILE_APPEND);
  if (logFile) {
    logFile.print("{\"ubicacion\":\"");
    logFile.print(ubicacion);
    logFile.print("\",\"accion\":\"");
    logFile.print(accion);
    logFile.print("\",\"hora\":\"");
    logFile.print(getFormattedTime());
    logFile.println("\"},");
    logFile.close();
  }
}

void logHoraSistema() {
  File logFile = SD.open("/logs.txt", FILE_APPEND);
  if (logFile) {
    logFile.print("{\"hora_sistema\":\"");
    logFile.print(getFormattedTime());
    logFile.println("\"},");
    logFile.close();
  }
}



// Aprovechamos tus variables globales ya existentes:
extern bool hora_valida;
extern int  localHour;
extern int  minute;

static Stream   *sOut = &Serial;
static LogLevel  sLevel = LOG_INFO;
static uint32_t  bootMs = 0;

static bool      wActive = false;
static const char *wName = nullptr;
static uint32_t  wStartMs = 0;

static const char* levelToStr(LogLevel lvl) {
  switch (lvl) {
    case LOG_ERROR: return "ERROR";
    case LOG_WARN:  return "WARN";
    case LOG_INFO:  return "INFO";
    case LOG_DEBUG: return "DEBUG";
    default:        return "";
  }
}

String logStamp() {
  uint32_t ms = millis() - bootMs;
  uint32_t s  = ms / 1000;
  uint32_t msRem = ms % 1000;

  char buf[32];
  snprintf(buf, sizeof(buf), "T+%04u.%03u", (unsigned)s, (unsigned)msRem);

  if (hora_valida) {
    // Añadimos HH:MM opcional. Los segundos los derivamos de millis para no depender de RTC.
    uint32_t sec = (millis() / 1000) % 60;
    char buf2[16];
    snprintf(buf2, sizeof(buf2), " | %02d:%02d:%02lu", localHour, minute, (unsigned long)sec);
    return String(buf) + String(buf2);
  }
  return String(buf);
}

void initLogger(Stream &out, LogLevel level) {
  sOut = &out;
  sLevel = level;
  bootMs = millis();
  logMark("BOOT");
  logMsg(LOG_INFO, "LOGGER", "Inicializado");
}

void setLogLevel(LogLevel level) {
  sLevel = level;
  logMsg(LOG_INFO, "LOGGER", String("Nivel: ") + levelToStr(level));
}

void logMsg(LogLevel level, const char *tag, const String &msg) {
  if (level > sLevel || level == LOG_SILENT) return;
  if (!sOut) return;

  String line = "[" + logStamp() + "] ";
  line += "[";
  line += levelToStr(level);
  line += "] ";
  line += tag;
  line += " — ";
  line += msg;

  sOut->println(line);
}

void logMark(const char *tag) {
  if (!sOut) return;
  sOut->println();
  sOut->println("==================================================");
  sOut->print  ("== ");
  sOut->print  (logStamp());
  sOut->print  (" | ");
  sOut->print  (tag);
  sOut->println(" ==");
  sOut->println("==================================================");
  sOut->println();
}

void startWindow(const char *name) {
  if (wActive) endWindow(wName);
  wActive = true;
  wName = name;
  wStartMs = millis();
  logMark(name);
  logMsg(LOG_INFO, "WINDOW", String("START: ") + name);
}

void endWindow(const char *name) {
  if (!wActive) return;
  uint32_t dur = millis() - wStartMs;
  logMsg(LOG_INFO, "WINDOW", String("END: ") + name + " | dur_ms=" + dur);
  wActive = false;
  wName = nullptr;
}

void heartbeat(const String &state) {
  // Mensaje corto y facilmente “grep-eable”
  String s = "hb";
  if (state.length()) {
    s += " | ";
    s += state;
  }
  logMsg(LOG_DEBUG, "HEARTBEAT", s);
}

