#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <Arduino.h>

bool initTelegram();                 
int  checkTelegram(unsigned long now);   // <-- devuelve int (nº de mensajes o -1)

void processCommand(const String& command, const String& chat_id);
bool chatIDAutorizado(const String& chat_id);

bool telegramSend(const String& msg);    // true si enviado OK
void sendTelegramMessage(const String& msg); // wrapper void para compatibilidad

// Bandera global para evitar duplicar bienvenida
extern bool welcomeSent;

// Helper idempotente (opcional)
bool sendWelcomeOnce();

#endif // TELEGRAM_H
