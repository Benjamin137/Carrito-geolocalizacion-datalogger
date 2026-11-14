# SX1278

Libreria privada para controlar un transceptor SX1278 en modo LoRa mediante
SPI. La libreria no se incluye ni se inicializa desde `main.ino`; queda lista
para una futura integracion con el gateway.

## Uso basico

```cpp
#include <SX1278.h>

SX1278 radio(5, 14, 2);

void setup() {
  radio.begin(433000000L);
  radio.setTxPower(17);
  radio.enableCrc(true);
  radio.send("hola");
}
```

`receive()` devuelve la cantidad de bytes recibidos, `0` cuando no hay un
paquete disponible, `-1` si el controlador no esta inicializado y `-2` si el
buffer es demasiado pequeno.

Los pines por defecto son CS=5, RESET=14, DIO0=2, SCK=18, MISO=19 y MOSI=23.
Deben ajustarse a la conexion real antes de instalar el modulo.
