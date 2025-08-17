#ifndef CONFIG_H
#define CONFIG_H

#include "config_user.h"  // Variables personalizables por el usuario

#define INTERVALO_CAN_READ 500
#define INTERVALO_CAN_CLOCK 200
#define INTERVALO_TELEGRAM 15000
#define INTERVALO_SMS 60000
#define INTERVALO_HORA_REINTENTO 30000

#define CAN_ID_SOC 0x540
#define CAN_ID_HORA 0x5A1

#define SOC_ALARMA 20
#define SOC_CRITICO 15

#define PIN_MODO_DIAGNOSTICO 12  // opcional, con pull-up

extern uint8_t soc;
extern bool hora_valida;
extern bool alerta_enviada;
extern bool apagado_ya;
extern bool red_activa;
extern bool modo_diagnostico;

#endif
