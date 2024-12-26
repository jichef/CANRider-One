# CANRider One

**CANRider One** es un sistema diseñado para integrar y supervisar datos de motos eléctricas mediante el protocolo CAN. Este proyecto combina simplicidad y funcionalidad, permitiendo:

- Monitoreo en tiempo real del estado de la batería.
- Localización del vehículo por GPS
- Registro de los datos en servidor propio de Traccar
- Comunicación mediante SMS

Ideal para entusiastas de la tecnología y la movilidad eléctrica, **CANRider One** ofrece una experiencia optimizada para comprender y mejorar el rendimiento de tu motocicleta.

## Características
- 📊 **Monitoreo en tiempo real**: Mantente informado sobre los datos clave de tu motocicleta.
- ⚡ **Conectividad CAN**: Utiliza el protocolo estándar para vehículos eléctricos.
- 🚀 **Fácil de usar**: Interfaz sencilla y enfoque en la funcionalidad.

## Motivación
Es conocido que con la actualizacion por parte se VMOTO de sus ECUs ha dejado sin servicio a muchos usuarios, obligando a adquirir una nueva ECU (y todos sabemos que no es barata). Su app, además, puede pasar mucho tiempo sin conexión. 

En mi caso, es muy importante conocer el porcentaje de la batería en cada momento, cosa que no siempre me acuerdo de comprobar. Gracias al desarrollo de [SusoDevs](https://github.com/Xmanu12/SuSoDevs/) descubrí que esto era posible gracias a la lectura del CANBus a traves de un ESP32.

Este software está desarrollado utilizando una placa LilyGo TSIM7000G. Basado en ESP32 y diseñado para aplicaciones de IoT. Está equipado con un módulo SIM7000G, que soporta comunicaciones GSM, GPRS, GNSS (GPS, GLONASS) y LTE CAT-M/NB-IoT, lo que permite conectividad móvil para transmisión de datos y ubicación. Por tanto, cuenta con soporte para tarjetas SIM.  Incluye ranura para tarjeta microSD y antenas para mejorar la recepción. Evidentemente, con Wifi y Bluetooth.

![LilyGo TSIM7000G](https://i0.wp.com/randomnerdtutorials.com/wp-content/uploads/2022/08/ESP32-TSIM7000G.jpg?resize=750%2C422&quality=100&strip=all&ssl=1$0)

La integración con Traccar viene definida (con algunos retoques) gracias al codigo de [github.com/onlinegill](https://github.com/onlinegill/LILYGO-TTGO-T-SIM7000G-ESP32-Traccar-GPS-tracker) y [github.com/markoAntonio1962](https://github.com/markoAntonio1692/TTGO-SIM7000G-TRACCAR).

## Estructura de Archivos: CANRider One
Hasta llegar a esta versión el código ha evolucionado. Lo he convertido en un programa modular, cada uno con propósitos diferentes, donde he dejado establecidas cuales son las variables para que se pueda adaptar el código a las necesadidades de cada uno.

A continuación, se detalla la descripción de cada archivo:

| Archivo           | Descripción                                                                 |
|-------------------|-----------------------------------------------------------------------------|
| `globals.cpp`     | Declaración e inicialización de variables globales del programa. Variables preparadas para que el usuario modifique.          |
| `messages.cpp`    | Decodificación y manejo de mensajes generales.             |
| `modem.cpp`       | Control y comunicación con el módem, incluyendo comandos AT.              |
| `sms.cpp`         | Implementación de la lógica para la gestión de mensajes SMS.               |
| `output.cpp`      | Gestión de la salida de datos, como logs o pantallas.                     |
| `twai.cpp`        | Implementación de la lógica para la gestión del bus TWAI (protocolo CAN). |
| `sms.h`           | Declaraciones de funciones relacionadas con SMS.                          |
| `globals.h`       | Declaraciones de variables globales para uso compartido entre módulos.    |
| `modem.h`         | Declaraciones de funciones para el manejo del módem.                      |
| `output.h`        | Declaraciones para funciones de salida de datos.                          |
| `messages.h`      | Declaraciones relacionadas con los mensajes CAN.                          |
| `twai.h`          | Declaraciones para funciones relacionadas con TWAI.                       |

# Configuración de Variables Globales

El archivo contiene las configuraciones principales para el dispositivo, como nombre Bluetooth, credenciales GSM, información del servidor Traccar, tiempos de actualización y palabras clave para comandos SMS. A continuación, se describe cada variable:

| **Variable**                  | **Alterable** | **Descripción**                                                                                          |
|-------------------------------|---------------|----------------------------------------------------------------------------------------------------------|
| `btDeviceName`                | ✔️            | Nombre por defecto del dispositivo Bluetooth (ej. `CANRider One`).                                            |
| `apn`                         | ✔️            | APN utilizado para la conexión GSM.                                                                     |
| `gprsUser`                    | ✔️            | Nombre de usuario para GPRS (si aplica).                                                                |
| `gprsPass`                    | ✔️            | Contraseña para GPRS (si aplica).                                                                       |
| `GSM_PIN`                     | ✔️            | PIN de la tarjeta SIM.                                                                                  |
| `server`                      | ✔️            | Dirección del servidor Traccar.                                                                         |
| `myid`                        | ✔️            | ID única configurada en Traccar para identificar el dispositivo.                                        |
| `VEHIEncendidoDelay`          | ✔️            | Tiempo de espera entre actualizaciones de GPS cuando hay alimentación USB (en milisegundos).            |
| `VEHIApagadoDelay`            | ✔️            | Tiempo de espera entre actualizaciones de GPS cuando no hay alimentación USB (en milisegundos).         |
| `phoneNumber`                 | ✔️            | Número de teléfono autorizado para recibir y enviar comandos SMS.                                       |
| `SMS_KEYWORD_SECURITY`        | ✔️            | Palabra clave para validar la autenticidad de los SMS recibidos. Solo se utiliza en el caso de enviarle un sms al ESP32 desde un teléfono que no es el nuetro (imagina que un amigo de lo ajeno se la ha llevado y no tienes acceso a tu número definido; o que alguien descubre el número de la SIM del ESP32 y te localiza la moto. En este caso solo responderá si el SMS incluye esta palabra de seguridad.                                        |
| `SMS_KEYWORD_GPS`             | ✔️            | Comando SMS para obtener la ubicación GPS en formato Google Maps.                                                              |
| `SMS_KEYWORD_REBOOT`          | ✔️            | Comando SMS para forzar un reinicio del dispositivo.                                                    |

---

## Notas sobre Alteraciones

- **Bluetooth**: Cambiar `btDeviceName` permite personalizar el nombre del dispositivo que será visible al emparejarlo.
- **GSM**: Las credenciales (`apn`, `gprsUser`, `gprsPass`, `GSM_PIN`) deben ser configuradas según el proveedor de servicios.
- **Servidor Traccar**: Cambiar `server` y `myid` según el servidor configurado y el ID asignado al dispositivo.
- **Tiempos de Actualización**: Ajustar `VEHIEncendidoDelay` y `VEHIApagadoDelay` según las necesidades de actualización del GPS.
- **Comandos SMS**: Las palabras clave (`SMS_KEYWORD_SECURITY`, `SMS_KEYWORD_GPS`, `SMS_KEYWORD_REBOOT`) pueden ser personalizadas para evitar colisiones o facilitar su uso.

Si necesitas más información o detalles sobre cómo ajustar alguna de estas variables, ¡puedes indicármelo! 😊


# Servidor de datos
Para obtener los servicios de localización no hace falta Tracar, pues podemos pedir por SMS y nos devolverá la ubicación en formato Google maps.
Para poder recibir los datos en local se necesita tener Traccar instalado. En mi instalación lo tengo ejecutándose en un LXC detrás de un servidor de Proxmox protegido por un proxy inverso. Te indico cómo configurarlo:

## Traccar

Mi instalación de Traccar corre en un contenedor de Proxmox. Pero puedes instalarlo en cualquier parte según la documentación de Traccar.

Igualmente, aquí te dejo los pasos para instalarlo en Proxmox:

[Proxmos Helper Scripts: Traccar en Proxmox](https://community-scripts.github.io/ProxmoxVE/scripts?id=traccar)

Copia este enlace en el shell de Proxmox

```bash
bash -c "$(wget -qLO - https://github.com/community-scripts/ProxmoxVE/raw/main/ct/traccar.sh)"
```

## Proxy inverso

Te recomiendo utilizar un proxy inverso para el cifrado https. Utilizo NGINX Proxy Manager. Hay muchas opciones de instalación, pero en mi caso lo tengo corriendo en un contenedor de Proxmox.

[Proxmos Helper Scripts: NGINX Proxy Manager](https://community-scripts.github.io/ProxmoxVE/scripts?id=nginxproxymanager)
```bash
bash -c "$(wget -qLO - https://github.com/community-scripts/ProxmoxVE/raw/main/ct/nginxproxymanager.sh)"
```
(Las credenciales de inicio de sesión son admin@example.com y pass: changeme; te pedirá cambiarlas con el primer inicio)

1. Bueno, imaginando que tienes NGINX Proxy Manager configurado y corriendo, lo primero que tenemos que hacer es darnos de alta en DuckDNS, y habilitar un nuevo subdominio (mi consejo es utilizar un nombre largo y aleatorio de letras y numeros, creo que se admiten hasta 30). Anota el token


| Variable  | Explicación |
| ------------- | ------------- |
| Domain names  | Escribe el nombre del subdominio.duckdns.org. |
| Schema  | Elige http |
| Hostname  | La dirección IP donde tienes Traccar funcionando |
| Port  | El puerto donde está funcionando (aquí deberías usar 5055) |
| Cache Assets  | Marcado |
| Websocket Support  | Marcado |
| Access List  | Public |

2. Pasamos a la pestaña SSL. En este momento vamos a generar un certificado para nuestro subdominio. Elegimos la opción Request a new certificate y marcamos "Use a DNS Challenge". En DNS Provider, busca DuckDNS y se descubrirá un apartado que se llama "Credentials File Content". En el cuadro de texto sustituye "your-duckdns-token" por el token que copiaste en el paso 1. desde la web DuckDNS.

2. Agrega un nuevo host. En el nombre del host escribe el nombre del subdominio.duckdns.org. 
3. Por ultimo, escribe el correo por el que estás registrado en Let's Encrypt (si no te has registrado, hazlo). Acepta los términos y dale a guardar. 

OJO. En la versión en la que me encuentro de Proxy Manager da error en el certbot para duckdns. He tenido que hacer SSH a la maquina de NGINX y ejecutar `pip install certbot-dns-duckdns`

4. No podemos habilitar dentro ninguna casilla ninguna casilla como Force SSL, porque Traccar solo acepta conexiones http (no he conseguido configurarlo para que acepte https). Dicho esto, podemos decir que nuestro acceso está protegido hasta llegar a nuestro router, pero cuando entramos en nuestra red local, ya viaja por http. Supongo que tu red local la consideras un espacio "seguro". 

Teniendo esto configurado podemos decir que nuestro acceso.

## Comandos de rescate
He decido agregar un plan B de rescate en el caso de que todo falle. Dado que el modem puede recibir, leer y enviar sms (qué viejo suena eso) puede ser una vía de comunicación alternativa con el modulo. El mensaje no importa cómo se envíe: mayúsculas, minúsculas, combinación... el texto recibido se convierte a minúsculas.

Los SMS entre números de teléfono DIGI son gratuitos, por tanto, no me preocupa el costo.


| Keyword  | Explicación |
| ------------- | ------------- |
| `reboot`  | Se recibe el mensaje y se envía una respuesta al remitente `El comando 'reboot' se ha recibido... reiniciando`. En ese momento el ESP32 se reinicia. |
| `gps`  | Se recibe el mensaje y se envía la última localización GPS en formato google maps: `Aqui esta tu localizacion: https://www.google.com/maps?q=LAT,LONG` |
| `vaciar`  | Elimina todos los sms del ESP. |

Los mensajes se consultan según los tiempos marcados en las variables `VEHIEncendidoDelay` o `VEHIapagadoDelay` para ahorrar batería. Una vez leídos son borrados.

# ¿Cómo funciona?
El ESP32 lleva una batería incorporada 18650, lo que le da una autonomía de unas 6 horas aproximadamente. Mientras que nuestra placa esté conectada por USB, la batería se carga; si deja de recibir carga por USB, se reinicia activándose el uso de la batería. 

Encontramos dos diferenciados flujos de trabajo. 

### Funcionamiento por USB 
Si está conectado por USB está recibiendo carga. Se da por supuesto que nuestro vehículo está arrancado y funcionando. El programa no es capaz de medir el voltaje de la bateria incorporada en nuestro ESP32 y da un valor de 0 y, como consecuencia, se envía a Traccar ignition=true. En el momento que se adquieren coordenadas gps a nuestro servidor según el tiempo definido por la variable `VEHIEncendidoDelay`. En Traccar se verá la ubicación del vehículo y el nivel de batería de la moto. 

### Funcionamiento por batería
Si no recibe carga por USB, el voltaje de la batería es calculado. Este se envía a Traccar y además un ignition=false. Sabremos por tanto el nivel de carga restante de nuestro ESP32, ubicación y batería de la moto. Se envían el tiempo definido por la variable `VEHIApagadoDelay`

## Licencia
Este proyecto está bajo la licencia [MIT](LICENSE).

¡Contribuciones y sugerencias son bienvenidas! 😊
