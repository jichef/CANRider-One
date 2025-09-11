#pragma once
#include <Arduino.h>

// === API pública del módulo Telegram ===
bool initTelegram();                     // Handshake TLS + prime
int  checkTelegram(unsigned long now);   // Poll periódico (devuelve nº msgs o -1 si error)
void processCommand(const String& command, const String& chat_id);
bool telegramSend(const String& msg);    // Envío al CHAT_ID principal
bool sendWelcomeOnce();                  // Mensaje de bienvenida (una sola vez)
bool chatIDAutorizado(const String& chat_id);

// Señal para otros módulos (p.ej., gps.cpp) de que el módem está ocupado por Telegram
extern "C" bool telegram_using_modem();
