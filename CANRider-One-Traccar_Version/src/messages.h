#ifndef MESSAGES_H
#define MESSAGES_H

// Mensajes de inicialización
const char INIT_MODEM[] = "Inicializando módem...";
const char MODEM_INIT_SUCCESS[] = "Módem inicializado correctamente.";
const char ERROR_MODEM_INIT[] = "Error al reiniciar el módem.";
const char CONFIGURING_MODEM[] = "Configurando el módem...";
const char MODEM_CONFIGURED[] = "Módem configurado correctamente.";
const char SYSTEM_INITIALIZED[] = "Sistema inicializado correctamente.";
const char MODEM_IMEI[] = "IMEI del módem: ";

// Mensajes de encendido y apagado del módem
const char MODEM_POWER_ON[] = "Encendiendo módem...";
const char MODEM_POWER_OFF[] = "Apagando módem...";
const char MODEM_RESTART[] = "Reiniciando módem...";

// Mensajes de red
const char NETWORK_PARAMS[] = "Parámetros actuales de red: ";
const char NETWORK_CHECK[] = "Verificando estado de la red...";
const char SIM_READY_MESSAGE[] = "SIM lista.";
const char SIM_NOT_READY_MESSAGE[] = "Advertencia: SIM no está lista.";
const char SIM_DENIED[] = "La conexión del SIM ha sido denegada.";
const char SIM_NO_COVERAGE[] = "No hay cobertura de red móvil.";
const char SIGNAL_LEVEL[] = "Nivel de señal: ";
const char CONNECTING_GPRS[] = "Conectando a la red GPRS...";
const char GPRS_SUCCESS[] = "Conexión GPRS exitosa.";
const char GPRS_FAIL[] = "Falló la conexión GPRS.";
const char GPRS_CONNECTION_FAILED[] = "Conexión GPRS fallida. Reiniciando...";
const char GPRS_APN[] = "Conectando con APN: ";
const char IP_ASSIGNED[] = "IP asignada: ";
const char GPRS_NO_IP[] = "Advertencia: No se pudo verificar la conexión GPRS.";

// Mensajes de GPS
const char GPS_START[] = "Activando GPS...";
const char GPS_STOP[] = "Desactivando GPS...";
const char GPS_ON[] = "GPS activado correctamente.";
const char GPS_OFF[] = "GPS desactivado correctamente.";
const char GPS_FAIL[] = "Error al desactivar el GPS.";
const char GPS_READING[] = "Obteniendo datos de GPS...";
const char GPS_ERROR[] = "No se pudieron obtener datos de GPS.";
const char GPS_LAT[] = "Latitud: ";
const char GPS_LON[] = "Longitud: ";
const char GPS_SPEED[] = "Velocidad: ";
const char GPS_ALT[] = "Altitud: ";
const char GPS_ACCURACY[] = "Precisión: ";
const char GPS_VSAT[] = "Satélites visibles: ";
const char GPS_USAT[] = "Satélites en uso: ";
const char GPS_DATE_TIME[] = "Fecha y hora GPS: ";
const char GPS_DATA_RETRY[] = "Fallo al obtener datos de GPS. Reintentando...";
const char GPS_LOCATION_SMS[] = "Ubicación actual: ";
const char GPS_LOCATION_SMS_FAILED[] = "Aún no han obtenido datos GPS";


// Mensajes de comandos AT
const char AT_COMMAND_SENT[] = "Comando AT enviado: ";
const char AT_RESPONSE[] = "Respuesta del comando AT: ";
const char AT_NO_RESPONSE[] = "No se recibió respuesta del comando AT.";
const char AT_ERROR[] = "Error en la ejecución del comando AT.";

// Mensajes de SMS
const char INIT_SMS_MODULE[] = "Inicializando módulo SMS...";
const char SMS_SENT_SUCCESS[] = "SMS enviado exitosamente.";
const char SMS_SEND_ERROR[] = "Error al enviar el SMS.";
const char SMS_PROMPT_ERROR[] = "Error: no se recibió el prompt '>' para enviar el SMS.";
const char DELETE_ALL_SMS[] = "Intentando borrar todos los SMS...";
const char DELETE_SMS_SUCCESS[] = "SMS borrado correctamente.";
const char DELETE_SMS_ERROR[] = "Error al borrar el SMS.";
const char ALL_SMS_DELETED[] = "Borrado de todos los SMS completado.";
const char CHECK_SMS[] = "Verificando SMS...";
const char NEW_SMS_RECEIVED[] = "Nuevo SMS recibido:";
const char SMS_CONTENT[] = "Contenido del SMS:";
const char NO_SMS_FOUND[] = "No se encontraron SMS nuevos.";
const char SMS_COMMAND_REBOOT_RECEIVED[] = "Comando REBOOT recibido por SMS.";
const char SMS_COMMAND_GPS_RECEIVED[] = "Comando GPS recibido por SMS.";
const char SMS_REBOOT_RESPONSE[] = "Reiniciando dispositivo...";
const char SMS_SENDING_TO[] = "Enviando SMS a ";
const char SMS_UNKNOWN_COMMAND[] = "SMS recibido sin comandos reconocidos.";

// Mensajes de TWAI
const char INIT_TWAI[] = "Inicializando el bus TWAI...";
const char INIT_TWAI_INSTALL_OK[] = "Driver de TWAI instalado correctamente.";
const char INIT_TWAI_INSTALL_ERROR[] = "Error al instalar el driver de TWAI.";
const char INIT_TWAI_SUCCESS[] = "Driver de TWAI iniciado correctamente.";
const char INIT_TWAI_ERROR[] = "Error al iniciar el driver de TWAI.";
const char INIT_TWAI_ALERT_ERR_PASS[] = "Alerta: Controlador TWAI en estado de error pasivo.";
const char INIT_TWAI_ALERT_BUS_ERROR[] = "Alerta: Error en el bus (Bit, Stuff, CRC, Form, ACK).";
const char INIT_TWAI_ERROR_COUNT[] = "Errores acumulados: ";
const char INIT_TWAI_MISSED_MSG[] = "Mensajes perdidos: ";
const char INIT_TWAI_ALERT_RX_QUEUE_FULL[] = "Alerta: Cola de recepción llena.";
const char INIT_TWAI_ID[] = "Mensaje recibido con ID: ";
const char INIT_TWAI_NULL[] = "Frame remoto recibido o sin datos.";
const char INIT_TWAI_UNKNOWN_ID[] = "ID de mensaje no reconocido.";
const char TWAI_SHUTDOWN[] = "Apagando el bus TWAI...";
const char TWAI_STOP_OK[] = "Driver de TWAI detenido correctamente.";
const char TWAI_STOP_ERROR[] = "Error al detener el driver de TWAI.";
const char TWAI_UNINSTALL_OK[] = "Driver de TWAI desinstalado correctamente.";
const char TWAI_UNINSTALL_ERROR[] = "Error al desinstalar el driver de TWAI.";

// Mensajes de batería
const char BATTERY_VOLTAGE_ESP[] = "Voltaje de la batería del ESP: ";
const char VEHI_ON[] = "Vehículo encendido. Enviando datos cada ";
const char VEHI_OFF[] = "Vehículo apagado. Enviando datos cada ";

// Mensajes de Traccar
const char SENDING_TO_TRACCAR[] = "Enviando datos a Traccar...";
const char TRACCAR_URL[] = "URL construida para Traccar.";
const char TRACCAR_CONNECTION_ERROR[] = "Error al conectar con el servidor de Traccar.";
const char TRACCAR_SUCCESS[] = "Datos enviados a Traccar correctamente.";
const char TRACCAR_HTTP_ERROR[] = "Error HTTP con código de respuesta.";
const char TRACCAR_CONNECTION_CLOSED[] = "Conexión HTTP cerrada.";
const char TRACCAR_FORCE_REBOOT[] = "Demasiados intentos fallidos. Reiniciando";
const char TRACCAR_INFO_REBOOT[] = "Reinicio forzado: demasiados errores consecutivos en conexión con Traccar";
#endif // MESSAGES_H
