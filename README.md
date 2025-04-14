# CANRider One

**CANRider One** es un sistema diseñado para integrar y supervisar datos de motos eléctricas mediante el protocolo CAN. Este proyecto combina simplicidad y funcionalidad, permitiendo:

- Monitoreo en tiempo real del estado de la batería.
- Localización del vehículo por GPS
- Registro de los datos en servidor propio de Traccar
- Comunicación mediante SMS

Ideal para entusiastas de la tecnología y la movilidad eléctrica, **CANRider One** ofrece una experiencia optimizada para comprender y mejorar el rendimiento de tu motocicleta.

Este proyecto está adaptado para SuperSoco CPX. 

Es un proyecto muy básico y, por supuesto, no me hago responsable de la que puedas liar ni de si te funciona. No debería ocurrir nada, pues en nigún momento el proyecto final envía tramas CAN a la moto (podría bloquear tu BMS). Lo he documentado lo mejor que he podido. 

## Características
- 📊 **Monitoreo en tiempo real**: Mantente informado sobre los datos clave de tu motocicleta.
- ⚡ **Conectividad CAN**: Utiliza el protocolo estándar para vehículos eléctricos.
- 🚀 **Fácil de usar**: Interfaz sencilla y enfoque en la funcionalidad.
- 🤖 **HomeAssistant**: Integrable en HomeAssistant gracias a la integración propia de Traccar

![Captura de Traccar](https://github.com/jichef/CANRider-One/blob/main/capture_traccar.png)

## Requisitos

- Vehículo que disponga de circuito CAN
- [LilyGo TSIM7000G 16MB (Aliexpress, también lo tienes en Amazon)](https://es.aliexpress.com/item/4000542688096.html?spm=a2g0o.productlist.main.3.32cbJudJJudJ2w&algo_pvid=415d3a53-2736-4e1c-81be-6b7a21f6e6fb&algo_exp_id=415d3a53-2736-4e1c-81be-6b7a21f6e6fb-1&pdp_npi=4%40dis%21EUR%2141.59%2141.59%21%21%2142.24%2142.24%21%4021038e6617352498647415124efd6a%2112000032432563392%21sea%21ES%210%21ABX&curPageLogUid=8nMRXncF299e&utparam-url=scene%3Asearch%7Cquery_from%3A) (aunque supongo que también valdrán otros, sólo he probado con el TSIM7670G -y no a fondo-)
- [Transciever SN65HVD230](https://www.amazon.es/gp/product/B07VG3Z9GT/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).
- Tarjeta SIM (Digi tiene una tarifa de datos con 2GB -que sobra- pero ofrece SMS gratis entre números DIGI)
- 🚨Servidor Traccar🚨. Este dato es importante tenerlo en cuenta. La localización GPS se graba en un servidor LOCAL. Tienes que disponer de esta infraestructura para poder utilizar este servicio. Hay muchas alternativas. Unas más seguras y otras menos: Raspberry Pi, NUC, Proxmox, servicio dedicado de Traccar... Yo he usado un NUC con Proxmox (abajo te explico cómo lo he montado).
- [Transciever SN65HVD230](https://www.amazon.es/gp/product/B07VG3Z9GT/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).
- [Carcasa impresa](https://www.thingiverse.com/thing:5861376)
- [ODB2 MACHO de 6 pin](https://es.aliexpress.com/item/1005004526698938.html?spm=a2g0o.order_list.order_list_main.5.445f194dxILzUf&gatewayAdapt=glo2esp).
- Conversor de [12V a 5V](https://www.amazon.es/gp/product/B0C3HDNT8R/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1) 

Entonces, ¿si no tengo servidor de Traccar, puedo conocer la ubicación GPS y estado de la batería, por ejemplo? Sí, claro que sí. Pero la obtendras por SMS, sé que es muy rudimentario, pero es la única forma de enviar los datos. Se podría enviar por Telegram, por ejemplo, pero eso podría estar en una lista TO-DO.

## Motivación
Es bien sabido que la actualización reciente por parte de VMOTO de sus ECUs ha dejado sin servicio a muchos usuarios, obligándolos a adquirir una nueva ECU (y todos sabemos lo costosas que son). Además, su app puede pasar largos períodos sin conexión.

En mi caso, es crucial conocer el porcentaje de la batería en todo momento, algo que no siempre recuerdo comprobar. Gracias al trabajo de [SusoDevs](https://github.com/Xmanu12/SuSoDevs/) descubrí que esto es posible a través de la lectura del CANBus con un ESP32.

CANRider One está basado en una placa [LilyGo TSIM7000G 16MB (Aliexpress)](https://es.aliexpress.com/item/4000542688096.html?spm=a2g0o.productlist.main.3.32cbJudJJudJ2w&algo_pvid=415d3a53-2736-4e1c-81be-6b7a21f6e6fb&algo_exp_id=415d3a53-2736-4e1c-81be-6b7a21f6e6fb-1&pdp_npi=4%40dis%21EUR%2141.59%2141.59%21%21%2142.24%2142.24%21%4021038e6617352498647415124efd6a%2112000032432563392%21sea%21ES%210%21ABX&curPageLogUid=8nMRXncF299e&utparam-url=scene%3Asearch%7Cquery_from%3A). Se trata de un dispositivo basado en ESP32, diseñado para aplicaciones IoT, que incluye un módulo SIM7000G compatible con GSM, GPRS, GNSS (GPS, GLONASS), y LTE CAT-M/NB-IoT. Esto permite la conectividad móvil para la transmisión de datos y ubicación. Además, del soporte para tarjetas SIM, dispone de ranura para microSD y antenas para mejorar la recepción. Y, por supuesto, cuenta con WiFi y Bluetooth.

![LilyGo TSIM7000G](https://i0.wp.com/randomnerdtutorials.com/wp-content/uploads/2022/08/ESP32-TSIM7000G.jpg?resize=750%2C422&quality=100&strip=all&ssl=1$0)

La integración con Traccar viene definida (con algunos retoques) gracias al codigo de [github.com/onlinegill](https://github.com/onlinegill/LILYGO-TTGO-T-SIM7000G-ESP32-Traccar-GPS-tracker) y [github.com/markoAntonio1962](https://github.com/markoAntonio1692/TTGO-SIM7000G-TRACCAR).

## Estructura de Archivos: CANRider One
A lo largo del proceso, el código ha pasado por varias etapas de evolución. He logrado transformarlo en un programa modular, donde cada módulo cumple con un propósito específico, haciéndolo más sencillo para mejorar las diferentes partes del proyecto. 

Además, he definido claramente las variables, lo que permite adaptar fácilmente el código a las necesidades particulares de cada usuario.

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

# Configuración de Variables Globales

El archivo ´globals.cpp´ contiene las configuraciones principales para el dispositivo, como nombre Bluetooth (que se mantiene por tener un log, terminará por desaparecer para ahorrar batería), credenciales GSM, información del servidor Traccar, tiempos de actualización y palabras clave para comandos SMS. A continuación, se describe cada variable:

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

La tarjeta SIM que tengo (DIGI) tiene SMS ilimitados entre números DIGI, por lo que me ha parecido mantener este servicio como un plan B en el caso de no tener conexión de datos y no preocuparme por el sobrecosto de los SMS.

# Servidor de datos
Para obtener y consultar los servicios de localización, no es necesario Traccar. Podemos solicitar la ubicación por SMS, y esta nos será devuelta en formato Google Maps. 

Sin embargo, para recibir los datos de forma local y llevar un registro sí es necesario tener Traccar instalado. En mi configuración, tengo Traccar ejecutándose en un contenedor LXC detrás de un servidor Proxmox, protegido por un proxy inverso. A continuación, te explico cómo configurarlo:

## Traccar

Mi instalación de Traccar se ejecuta en un contenedor de Proxmox, pero puedes instalarlo en cualquier otro entorno siguiendo la documentación oficial de Traccar.

De todos modos, aquí te dejo los pasos para instalarlo en Proxmox:

[Proxmos Helper Scripts: Traccar en Proxmox](https://community-scripts.github.io/ProxmoxVE/scripts?id=traccar)

Copia este enlace en el shell de Proxmox

```bash
bash -c "$(wget -qLO - https://github.com/community-scripts/ProxmoxVE/raw/main/ct/traccar.sh)"
```

## Proxy inverso

Te recomiendo utilizar un proxy inverso para habilitar el cifrado HTTPS. En mi caso, utilizo NGINX Proxy Manager, aunque existen varias opciones de instalación. Yo lo tengo ejecutándose en un contenedor de Proxmox.

Si te interesa, puedes seguir los pasos para instalarlo con este script.

[Proxmos Helper Scripts: NGINX Proxy Manager](https://community-scripts.github.io/ProxmoxVE/scripts?id=nginxproxymanager)
```bash
bash -c "$(wget -qLO - https://github.com/community-scripts/ProxmoxVE/raw/main/ct/nginxproxymanager.sh)"
```
(Las credenciales predeterminadas son user ´admin@example.com´ y pass ´changeme´; el sistema te pedirá que las cambies al primer inicio).

1. Asumiendo que ya tienes NGINX Proxy Manager configurado y en funcionamiento, lo siguiente es registrarte en DuckDNS y habilitar un nuevo subdominio (mi recomendación es usar un nombre largo y aleatorio de letras y números, ya que se admiten hasta 30 caracteres). No olvides guardar el token.


| Variable  | Explicación |
| ------------- | ------------- |
| Domain names  | Escribe el nombre del subdominio.duckdns.org. |
| Schema  | Elige http |
| Hostname  | La dirección IP donde tienes Traccar funcionando |
| Port  | El puerto donde está funcionando (aquí deberías usar 5055) |
| Cache Assets  | Marcado |
| Websocket Support  | Marcado |
| Access List  | Public |

2. Ahora vamos a la pestaña SSL para generar un certificado para nuestro subdominio. Selecciona la opción "Request a new certificate" y marca "Use a DNS Challenge". En el campo DNS Provider, elige DuckDNS. Aparecerá un apartado llamado "Credentials File Content". En el cuadro de texto, reemplaza "your-duckdns-token" con el token que copiaste en el paso 1 desde la web de DuckDNS.

3. A continuación, agrega un nuevo host. En el campo de nombre del host, escribe el subdominio que creaste, es decir, subdominio.duckdns.org.

4. Por último, ingresa el correo con el que te has registrado en Let's Encrypt (si aún no te has registrado, hazlo). Acepta los términos y haz clic en Guardar.

IMPORTANTE: En la versión de Proxy Manager que estoy utilizando, encontré un error con el certbot para DuckDNS. Tuve que acceder a la máquina de NGINX a través de SSH y ejecutar el siguiente comando: `pip install certbot-dns-duckdns` (también puedes acceder desde Proxmox).

5. No podemos habilitar ninguna casilla como Force SSL, ya que Traccar solo acepta conexiones HTTP (hasta ahora no he logrado configurarlo para que acepte HTTPS). Dicho esto, podemos asegurar que el acceso está protegido hasta llegar a nuestro router, pero una vez dentro de la red local, la conexión ya se realiza por HTTP. Supongo que tu red local la consideras un espacio "seguro".

6. Con esta configuración, podemos afirmar que nuestro acceso está protegido mientras atraviesa la red externa, pero en el interior de nuestra red local, la comunicación sigue siendo HTTP.

## Comandos de rescate
He decidido agregar un plan B de rescate en caso de que todo falle. Dado que el módem puede recibir, leer y enviar SMS (¡qué retro suena eso!), se puede utilizar como una vía de comunicación alternativa con el módulo. No importa cómo se envíe el mensaje: mayúsculas, minúsculas o una combinación de ambas; el texto recibido se convertirá automáticamente a minúsculas.

Además, como los SMS entre números de teléfono DIGI son gratuitos, no tengo que preocuparme por los costos.


| Keyword  | Explicación |
| ------------- | ------------- |
| `reboot`  | Se recibe el mensaje y se envía una respuesta al remitente `El comando 'reboot' se ha recibido... reiniciando`. En ese momento el ESP32 se reinicia. |
| `gps`  | Se recibe el mensaje y se envía la última localización GPS en formato google maps: `Aqui esta tu localizacion: https://www.google.com/maps?q=LAT,LONG` |
| `vaciar`  | Elimina todos los sms del ESP. |

Los mensajes se consultan según los tiempos marcados en las variables `VEHIEncendidoDelay` o `VEHIapagadoDelay` para ahorrar batería. Una vez leídos son borrados.

Además, solo responden a un número de teléfono defindo en una variable del código (en globals.cpp ´phoneNumber´). Sí un número desconocido pide localización GPS el ESP no responderá a no ser que incluya una palabra secreta definida también en otra variable del código (´SMS_KEYWORD_SECURITY´].

# ¿Cómo funciona?
El ESP32 cuenta con una batería opcional 18650, lo que le proporciona una autonomía de aproximadamente 6 horas. Mientras la placa esté conectada por USB, la batería se cargará; en caso de que deje de recibir carga por USB, se reiniciará y se activará el uso de la batería.

De esta manera, encontramos dos flujos de trabajo diferenciados:

### Funcionamiento por USB 
Si está conectado por USB está recibiendo carga. Se da por supuesto que nuestro vehículo está arrancado y funcionando. El programa no es capaz de medir el voltaje de la bateria incorporada en nuestro ESP32 y da un valor de 0 y, como consecuencia, se envía a Traccar ignition=true. En el momento que se adquieren coordenadas gps a nuestro servidor según el tiempo definido por la variable `VEHIEncendidoDelay`. En Traccar se verá la ubicación del vehículo y el nivel de batería de la moto. 

### Funcionamiento por batería
Si no recibe carga por USB, el voltaje de la batería es calculado. Este se envía a Traccar y además un ignition=false. Sabremos por tanto el nivel de carga restante de nuestro ESP32, ubicación y batería de la moto. Se envían el tiempo definido por la variable `VEHIApagadoDelay`

# ¿Cómo compilarlo?
Descarga la carpeta SRC completa y abre CanRiderONE.ino con Arduino. Descarga la librería ´Adafruit_ST7735´ y ´Adafruit_GFX´. Realmente no son necesarias, pero en la versión que tengo uso una pequeña pantalla TFT para poder ver los logs. Puedes eliminar las referencias del código si quieres.

Adapta las variables recogidas en ´globals.cpp´ y compila. 

Estoy seguro que se puede usar con otros módems, por lo que en ´globals.cpp´puedes modificar los pines de marcado del módem para poder usarlo.

Compila. 

## Conexiones
Además, evidentemente necesitas conectar todo. Necesitas un [transciever SN65HVD230](https://www.amazon.es/gp/product/B07VG3Z9GT/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) (ojo, no recomiendan comprarlo en Aliexpress). En este momento tienes dos opciones. Primero ¿tienes una ECU? si ya tienes una tienes que saber que es la encargada de mandar la hora al display. Si quieres mantener la hora debes dejarla conectada; en este caso tendras que cortar una resistencia a tu transceiver (da igual si es R1 o R2). En el caso que no dispongas de ECU o simplemente te importe un carajo la hora en el display NO ELIMINES NINGUNA resistencia.

Se utilizan los pines del ESP32 32 para TX y 33 para RX del transciever. Están elegidos esos para que queden juntos 3.3V, GND, 32 y 33. Directos al transciever. El CANL y H del mismo deberan conectarse a la moto. 

![Trasnceiver-tsim](https://github.com/jichef/CANRider-One/blob/main/images/transciever-tsim.jpg)

¡Ah! Me olvidaba. En thingiverse.com tienes la carcasa muy chula para imprimirla en 3D.

[Carcasa impresa](https://www.thingiverse.com/thing:5861376)

Para poder conectar el CANBus a la moto puedes hacerlo como bien dije anteriormente de dos maneras. La más cómoda es detrás, pues así mantienes la hora (dejando la ECU original conectada). Para ello he comprado una clavija [ODB2 MACHO de 6 pin](https://es.aliexpress.com/item/1005004526698938.html?spm=a2g0o.order_list.order_list_main.5.445f194dxILzUf&gatewayAdapt=glo2esp). Solo son necesarios el PIN6 que es el + de 12V y el GND que es el PIN1; además el CANH PIN2 y el CANL el PIN5. La salida es de 12V, por tanto, compré además este conversor de [12V a 5V](https://www.amazon.es/gp/product/B0C3HDNT8R/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1) para alimentar el ESP32.

La clavija ODB2 no cabe en el compartimento pues viene con una pestañita en su parte inferior que he cortado:

![ODB2](https://github.com/jichef/CANRider-One/blob/main/images/photo_2_2025-01-08_23-35-06.jpg)

Tras el corte queda algo así:

![ODB2_modificado](https://github.com/jichef/CANRider-One/blob/main/images/photo_1_2025-01-08_23-35-06.jpg)

Una vez conectado todo... Voilá!
![Running_CANRider](https://github.com/jichef/CANRider-One/blob/main/images/photo_1_2025-01-08_23-33-56.jpg)
![Running_CANRider_detail](https://github.com/jichef/CANRider-One/blob/main/images/photo_2_2025-01-08_23-33-56.jpg)

Solo queda abrir algunos agujerillos con el taladro para poder pasar los cables perfectamente sin que queden por fuera para dejarlo un poco más estético.

## Velocidad del bus
La velocidad del bus de Supersoco CPX es 250kbps, pero si deseas usar este proyecto y no sabes la velocidad del bus de tu vehículo, te dejo un [.ino](ReceptorTWAI_speed/Receptor_TWAI_multiplespeed.ino) que escucha a diferentes velocidades. Empieza en 125kbps y cada 5 segundos aumenta la velocidad hasta 1000Kbps mientras informa de lo que recibe. Después puedes usar este otro [receptor TWAI](ReceptorTWAI_speed/Receptor_TWAI.ino) con la velocidad que elijas para escuchar todo lo que se cuece por tu circuito CAN.

## Licencia
Este proyecto está bajo la licencia [MIT](LICENSE).

¡Contribuciones y sugerencias son bienvenidas! 😊
