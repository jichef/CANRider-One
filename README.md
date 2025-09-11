# CANRider-One. Actualización para Telegram

He decidido dejar a un lado la versión de Traccar porque requiere un mayor desarrollo y aparataje que en realidad no era necesario. Quedaba muy bien pero era demasiado engorroso para quienes empiezan de cero. Simplificando el proceso con Telegram, se reduce a preguntarle a un bot por el estado de la batería del vehículo y ubicación.

📡 Proyecto basado en ESP32 con conectividad GSM (SIM7000G), Bluetooth, GPS y comunicación CAN bus para motos eléctricas como la Supersoco CPX.

Este firmware permite:
- Enviar mensajes por **Telegram** para consultar batería, ubicación GPS
- Recibir y procesar tramas **CAN** desde el sistema de la moto.
- Optimización básica del consumo energético (GPS apagado por defecto, GPRS siempre activo pero optimizado).

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
# 🏍️ CanRIDER

CanRIDER es un sistema basado en **ESP32 + módem SIM7000** diseñado para integrarse con la motocicleta **Supersoco CPX** mediante el bus **CAN (TWAI a 250 kbps)**.  
Permite monitorizar el estado de la batería (SoC), enviar la hora al bus, registrar datos en microSD y controlar el sistema mediante **Telegram**.

---

## ⚙️ Hardware necesario

- **ESP32** (ej. DevKit, LilyGO T7000, MH-ET Live Mini, etc.)
- **Transceptor CAN** (SN65HVD230 u otro compatible)
  - `TX_GPIO = 33`
  - `RX_GPIO = 32`
- **Módem SIM7000G** con tarjeta SIM (APN configurable)
- **MicroSD** (opcional, para logs en JSON)
- **Conversor DC-DC** (60 V → 5 V) para alimentación desde la moto

---

## 📚 Librerías

Instalar en Arduino IDE / PlatformIO:

- [TinyGSM](https://github.com/vshymanskyy/TinyGSM)  
- `TinyGsmClientSIM7000SSL` (HTTPS para SIM7000)  
- [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)  
- ArduinoJson  
- ESP32 CAN (TWAI, incluido en ESP32 core)  
- FreeRTOS (incluido en ESP32 core)  
- SD (incluido en ESP32 core)

---

## 🔌 Conexiones

- **CAN-H / CAN-L** → bus CPX  
- **ESP32 GPIO 33** → TX transceptor  
- **ESP32 GPIO 32** → RX transceptor  
- **UART SIM7000** → ESP32 (según tu placa)  
- **Alimentación**: 5 V a ESP32 y SIM7000

---

## 🛠️ Instalación

```bash
git clone https://github.com/tu_usuario/CanRIDER.git
cd CanRIDER
```

1. Abre el proyecto en Arduino IDE o PlatformIO.  
2. Configura el archivo `config_user.h`:

```cpp
#define APN "orangeworld"      // Ejemplo Yoigo
#define BOT_TOKEN "123456:ABC..."  // Token de tu bot Telegram
#define CHAT_ID  "123456789"       // chat_id autorizado
```

3. Compila y sube el firmware al ESP32.

---

## 📡 Funcionalidades

### 🔋 Estado de batería (SoC) 
- Escucha tramas **CAN ID `0x541`**  
- El primer byte (`data[0]`) indica el % de batería  
- Se muestra por Telegram y se registra en SD si está insertada

### ⏰ Hora
- Se obtiene del módem (`+CCLK?`)  
- Se envía al bus CAN cada **200 ms** 


### 📍 GPS
- El SIM7000 activa GNSS bajo demanda  
- Responde con enlace de Google Maps (`/gps`)

### 🗂️ Logs
- Guardados en **`logs_YYYY-MM-DD.txt`** en SD  
- Ejemplo:

```json
{"hora_sistema":"12:34:56","soc":55,"evento":"encendido"}
```

---

## 🤖 Comandos de Telegram

| Comando     | Descripción |
|-------------|-------------|
| `/start`    | Mensaje de bienvenida |
| `/hora`     | Hora actual del sistema |
| `/estado`   | Estado del sistema (SoC, red, hora, barra gráfica de batería) |
| `/gps`      | Coordenadas actuales |

> ⚠️ Solo responden los `chat_id` autorizados en lista blanca.

---

## 🛡️ Seguridad y fiabilidad

- Lista blanca de `chat_id`  
- Logs críticos en microSD  
- Watchdog → reinicio de módem en caso de fallo  

---

## 📊 Ejemplo en Telegram

```
⚡ Estado del sistema
🔋 Batería: 55%
[█████░░░░░]
📡 GNSS: desactivado
⏰ Hora: 12:34
```
---
## Con `/gps` →  
📍 [Abrir en Google Maps](https://maps.google.com/?q=37.123,-3.456)
Cuando envías /gps por Telegram, el sistema: activa el GNSS del SIM7000, busca fix de satélites hasta un timeout (10 minutos), va informando por Telegram del progreso (cada ~25–30 s se recibe un mensaje indicando el progreso de satélites encontrados; al princpio busca sateítes y es muy exigente con la localización, si pasad un tiempo no mejora, empieza a bajar el nivel de exigencia enviando posibles ubicaciones; si tras los 10 min encuentra algo mejor, lo envía), 
devuelve la ubicación como enlace de Google Maps (lat,lon) y, si está disponible, precisión/datos útiles. Finalmente apaga el GNSS para ahorrar energía.

---
## 🚦 Roadmap

- [x] Envío de hora por CAN  
- [x] Lectura de SoC  
- [x] Control por Telegram  
- [x] Logs en microSD  
- [ ] Geocercas  
- [ ] Panel web con Husarnet  

---

## 📜 Licencia

MIT License — Libre para usar y modificar.

---

✍️ **Autor:** [@tu_usuario](https://github.com/tu_usuario)  
📌 Proyecto para la Supersoco CPX
