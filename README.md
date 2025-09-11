# 🏍️ CanRIDER

CANRider One es un sistema diseñado para integrar y supervisar datos de motos eléctricas mediante el protocolo CAN. Este proyecto combina simplicidad y funcionalidad, permitiendo:
Permite monitorizar el estado de la batería (SoC), enviar la hora al bus, registrar datos en microSD y controlar el sistema mediante **Telegram**.
Ideal para entusiastas de la tecnología y la movilidad eléctrica, CANRider One ofrece una experiencia optimizada para comprender y mejorar el rendimiento de tu motocicleta.

Este proyecto está adaptado para SuperSoco CPX.

Es un proyecto muy básico y, por supuesto, no me hago responsable de la que puedas liar ni de si te funciona. No debería ocurrir nada, pues en nigún momento el proyecto final envía tramas CAN diferentes a la hora a la moto (podría bloquear tu BMS). Lo he documentado lo mejor que he podido.

# CANRider-One. Versión para Telegram

He decidido dejar a un lado la [versión de Traccar - puedes consultarla aquí-](https://github.com/jichef/CANRider-One/tree/main/CANRider-One-Traccar_Version) porque requiere un mayor desarrollo y aparataje que en realidad no era necesario. Quedaba muy bien pero era demasiado engorroso para quienes empiezan de cero con poca idea. Simplificando el proceso con Telegram, se reduce a preguntarle a un bot por el estado de la batería del vehículo y ubicación. Fin. 

Solo necesitas comprar el ESP32, transceptor, carcasa, compilar y listo.

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

---

## ⚙️ Hardware necesario

- **ESP32** [LilyGO T7000G (Importante la G) ](https://es.aliexpress.com/item/4000542688096.html?spm=a2g0o.productlist.main.3.32cbJudJJudJ2w&algo_pvid=415d3a53-2736-4e1c-81be-6b7a21f6e6fb&algo_exp_id=415d3a53-2736-4e1c-81be-6b7a21f6e6fb-1&pdp_npi=4%40dis%21EUR%2141.59%2141.59%21%21%2142.24%2142.24%21%4021038e6617352498647415124efd6a%2112000032432563392%21sea%21ES%210%21ABX&curPageLogUid=8nMRXncF299e&utparam-url=scene%3Asearch%7Cquery_from%3A)
- **Transceptor CAN** [SN65HVD230 u otro compatible](https://www.amazon.es/gp/product/B07VG3Z9GT/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
  - `TX_GPIO = 33`
  - `RX_GPIO = 32`
- **Módem SIM7000G** con tarjeta SIM (APN configurable)
- **MicroSD** (opcional, para logs en JSON)
- **Conversor DC-DC** (60 V → 5 V) para alimentación desde la moto sustituyendo la ECU original
- **Tarjeta SIM** (Digi tiene una tarifa de datos con 2GB -que sobra- pero ofrece SMS gratis entre números DIGI)
- **Carcasa impresa** https://www.thingiverse.com/thing:5861376

---

## 📚 Librerías

Descargar Arduino IDE e instalar en Arduino IDE:

- [TinyGSM](https://github.com/vshymanskyy/TinyGSM)  
- `TinyGsmClientSIM7000SSL` (HTTPS para SIM7000, está incluída en la anterior)  
- [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)  
- ArduinoJson  
- ESP32 CAN (TWAI, incluido en ESP32 core)  
- FreeRTOS (incluido en ESP32 core)  
- SD (incluido en ESP32 core)

Todo es es sencillo: "Manage Libraries" y ahí buscar e instalarlas. 

Descargar el proyecto y compilar
---

## 🔌 Conexiones
Del transceptor:
- **CAN-H / CAN-L** del transceptor → bus CPX (conector delantero de la ECU, adjuntaré fotos) 
- **ESP32 GPIO 33** → TX transceptor  
- **ESP32 GPIO 32** → RX transceptor
- **Alimentación**: 5 V a TSIM7000
- **GND Transceptor** GND del TSIM7000 
---

## 🛠️ Instalación
Descagar el ZIP

1. Abre el proyecto en Arduino IDE  
2. Configura el archivo `config_user.h`:

```cpp
#define APN "internet.digimobile.es"      // Ejemplo DIGI
#define BOT_TOKEN "123456:ABC..."  // Token de tu bot Telegram
#define CHAT_ID  "123456789"       // chat_id autorizado
```
NOTA: Tienes que crear un bot y conseguir el token. Es sencillo, googlea y verás.

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

✍️ **Autor:** [@tjichef](https://github.com/jichef)  
📌 Proyecto para la Supersoco CPX
