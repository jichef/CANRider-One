#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <driver/twai.h>

bool isTWAIStarted();
void logTWAIStatus(const char* tag = "TWAI");
void initCAN();
void checkCANInput();
void sendHourViaCAN(int hour, int minute);
void taskSendCANHour(void* pvParameters);
void taskCANProcessing(void* pvParameters);

#endif