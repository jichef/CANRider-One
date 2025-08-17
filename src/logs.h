#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>

void initSD();
void logEvento(const String& evento);
void logUbicacion(const String& lat, const String& lon);
void logHora(const String& hora);

#pragma once
#include <Arduino.h>

enum LogLevel { LOG_SILENT=0, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

void initLogger(Stream &out = Serial, LogLevel level = LOG_INFO);
void setLogLevel(LogLevel level);

// Mensaje general: logMsg(LOG_INFO, "GPRS", "Conectado");
void logMsg(LogLevel level, const char *tag, const String &msg);

// Marca visual grande: logMark("GPS_ON");
void logMark(const char *tag);

// Ventanas de medida (para correlacionar con el medidor)
void startWindow(const char *name);  // startWindow("GPS_FIX_SEARCH");
void endWindow(const char *name);    // endWindow("GPS_FIX_SEARCH");

// Pulso periódico con estado (opcional, cada 5–10 s)
void heartbeat(const String &state = "");

// Sello temporal "T+0012.345 | 14:46:22" (si hay hora válida)
String logStamp();

#endif
