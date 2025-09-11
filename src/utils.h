#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

String urlencode(const String &str);
void logToOutput(const String& msg);
void logToOutputln(const String& msg);

#endif
