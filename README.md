# CANRider-One. Actualización para Telegram

He decidido dejar a un lado la versión de Traccar porque requiere un mayor desarrollo y aparataje que en realidad no era necesario. Quedaba muy bien pero era demasiado engorroso para quienes empiezan de cero. Simplificando el proceso con Telegram, se reduce a preguntarle a un bot por el estado de la batería del vehículo y ubicación.

📡 Proyecto basado en ESP32 con conectividad GSM (SIM7000G), Bluetooth, GPS y comunicación CAN bus para motos eléctricas como la Supersoco CPX.

Este firmware permite:
- Enviar mensajes por **Telegram** para consultar batería, ubicación GPS o encender Bluetooth.
- Recibir y procesar tramas **CAN** desde el sistema de la moto.
- Consultar información de forma remota mediante **SMS** o Telegram.
- Optimización básica del consumo energético (GPS y Bluetooth apagados por defecto, GPRS siempre activo pero optimizado).

---

## 🚀 Características principales

- ✅ Comunicación por **GPRS (HTTPS)** usando `TinyGsmClientSIM7000SSL`.
- ✅ Soporte para **mensajes SMS** entrantes.
- ✅ Lectura de ubicación **GPS** bajo demanda.
- ✅ Lectura de batería del ESP32 y batería CAN del vehículo.
- ✅ Gestión remota vía **Telegram bot**.
- ✅ Activación y desactivación de **Bluetooth** mediante comandos.
- ✅ Soporte para **TWAI (CAN Bus)** con recepción de tramas.

---

## 📝 Comandos disponibles por Telegram

Envía estos mensajes al bot autorizado:

| Comando            | Acción                                                   |
|--------------------|----------------------------------------------------------|
| `/gps`             | Activa GPS temporalmente y responde con enlace de ubicación |
| `/bateria`         | Devuelve nivel de batería del vehículo (por CAN)        |
| `/bateriaesp32`    | Devuelve el voltaje y porcentaje estimado del ESP32     |
| `/bluetooth_on`    | Activa el módulo Bluetooth                               |
| `/bluetooth_off`   | Apaga el módulo Bluetooth                                |
| `/reboot`          | Reinicia el dispositivo                                  |

---

## 🔌 Gestión energética

- **GPS**: solo se activa al pedir ubicación vía comando.
- **Bluetooth**: desactivado por defecto; se activa solo a petición.
- **GPRS**: permanece activo pero no se reconecta constantemente.
- **SMS**: revisión cada 60 segundos.
- (En futuras versiones se evaluará introducir *light sleep*).

---

## 🧾 Estructura del proyecto

