# Esta clase manipula completamente al carrito

La clase `Carrito` recibe por referencia dos objetos `DRV8833`: uno para el lado
izquierdo y otro para el lado derecho. Cada driver controla el motor delantero
y trasero de su lado.

La clase se encuentra en `src/carrito.h` y expone:

- `begin()`
- `setVelocidad()` y `getVelocidad()`
- `avanzar()`
- `reversa()`
- `girarDerecha()`
- `girarIzquierda()`
- `detener()`

Las maniobras aceptan opcionalmente una velocidad entre 0 y 255. Para evitar
conflictos de PWM, los dos objetos `DRV8833` deben usar rangos de cuatro
canales distintos; por ejemplo, canales 0-3 para el lado izquierdo y 4-7 para
el lado derecho.
