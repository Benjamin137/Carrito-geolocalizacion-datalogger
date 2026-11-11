# El proyecto sera para un carrito TT


# Funcionalidad
● Ubicación: obtener la posición GPS, para almacenar la información en un
datalogger y gestionar el robot mediante el sitio web.
● Comunicación: comunicación bidireccional con el Gateway mediante un
módulo LoRa.
● Seguidor de línea: capaz de seguir una pista diseñada por el docente.
● Evitador de obstáculos: detectar y evitar colisiones usando un sensor
ultrasónico.
● Control manual: permitir que un usuario controle el robot en tiempo real
mediante una aplicación web usando websockets por MQTT, el carrito no usara MQTT, eso lo enviara al gateway y que el gateway se encargue de eso.

Universidad de Colima

Nombre del Documento Fecha Implantación Edición Clave Fecha de revisión.
Práctica II - 01 FT-RG-13

5
● Control automático: navegar hasta un punto del mapa indicado por el usuario
mediante un mapa interactivo mediante un sitio web empleando websockets
por MQTT.
● Datalogger: registrar eventos y acciones en apartados separados en la
microSD, principalmente ubicación, presión atmosférica y temperatura, hora y
fecha.


## Nota importante

El carrito se comunica con un gateway. La informacion de sensores, movimiento y
comunicacion debe poder registrarse con fecha y hora en la microSD.

## Conocimiento actual del proyecto

### Clases disponibles

| Funcion | Clase | Estado y uso |
|---|---|---|
| Pantalla OLED | `OLEDI2C` en `src/Oled.h` | Disponible. Se usa para mostrar mensajes y logs en lugar de `Serial`. Usa I2C; por defecto SDA = 21 y SCL = 22. |
| Reloj RTC | `RTCClock` en `src/RTCClock.h` | Disponible. Actualmente hereda de `RTC_DS1307` y proporciona hora, fecha y formatos de texto. Usa I2C; por defecto SDA = 21 y SCL = 22. |
| MicroSD | `SDModule` en `src/SDModule.h` | Disponible. Usa SPI por defecto: CS = 5, SCK = 18, MISO = 19 y MOSI = 23. Permite escribir, agregar líneas, leer archivos y consultar espacio. |
| Temperatura y humedad | `DHT22Sensor` en `src/DHT22Sensor.h` | Disponible solo para DHT22. Las lecturas son no bloqueantes y deben actualizarse con `update()` cada vuelta del `loop()`. No reemplaza un AHT ni un BMP. |
| Motores | `DRV8833` en `src/DRV8833.h` | Disponible para controlar los canales del driver DRV8833. |
| Movimiento del carrito | `Carrito` en `src/carrito.h` | Disponible. Recibe dos `DRV8833` por referencia: lado izquierdo y lado derecho. Controla avanzar, reversa, giros y detención. |
| WiFi | `Wifi_Connect` en `src/Wifi_Connect.h` | Disponible para conexión al gateway por red WiFi, si ese será el transporte elegido. |
| MQTT | `MQTT_Controller` en `src/MQTT_Controller.h` | Disponible para publicar y recibir mensajes mediante MQTT. Puede servir como comunicación con el gateway por WiFi. |
| LED RGB | `RGBLed` en `src/RGBLed.h` | Disponible para indicar estados del carrito. |
| Buzzer | `Buzzer` en `src/Buzzer.h` | Disponible para alertas acústicas. |

## Reglas funcionales

- La OLED se reserva para logs y estados visibles del carrito; el registro
  permanente debe escribirse en la microSD.
- Cada evento guardado debe incluir, cuando sea posible, fecha y hora del RTC,
  tipo de evento, estado de movimiento y datos relevantes del sensor.
- El RTC solo proporciona fecha y hora; no reemplaza la fecha/hora del GPS.
- La prevención de colisiones tiene prioridad sobre las órdenes de navegación.
- El modo de seguimiento de línea debe poder activarse como modo autónomo sin
  perder el registro de eventos.
- Antes de instalar el hardware se deben confirmar modelo exacto, pines, niveles
  eléctricos y frecuencia de cada módulo. Las clases nuevas usan valores por
  defecto, pero deben ajustarse a la conexión real.
