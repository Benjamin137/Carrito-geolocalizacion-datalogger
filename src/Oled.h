#ifndef OLEDI2C_H
#define OLEDI2C_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Clase para pantalla OLED SSD1306 por I2C (128x64 por defecto) en ESP32, todo en un solo .h.
// Tiene la MISMA API que LCDI2C (printLine, printCentered, iconos, spinner, barra...),
// así que puedes cambiar de pantalla casi sin tocar el programa.
//
// HEREDA de Adafruit_SSD1306 (y por tanto de Adafruit_GFX), por lo que conservas todas
// sus funciones originales: drawLine(), drawRect(), drawCircle(), drawBitmap(),
// invertDisplay(), setRotation(), print() con coordenadas en píxeles, etc.
//
// DIFERENCIAS con el LCD:
//  - La OLED es gráfica: el texto se divide en "filas" y "columnas" según el tamaño de letra
//    (escala 1 = 6x8 px por carácter -> 21 columnas x 8 filas en 128x64).
//  - Los cambios se envían a la pantalla automáticamente (display()). Si quieres dibujar
//    mucho y enviar una sola vez, usa setAutoDisplay(false) y luego refresh().
//  - Los iconos no tienen el límite de 8 del LCD: están todos disponibles a la vez.
class OLEDI2C : public Adafruit_SSD1306 {
public:
  // ---------------- Caracteres especiales (fuente CP437 de la librería) ----------------
  static const char ICON_DEGREE      = (char)0xF8;  // °
  static const char ICON_ARROW_RIGHT = (char)0x1A;  // →
  static const char ICON_ARROW_LEFT  = (char)0x1B;  // ←
  static const char ICON_BLOCK       = (char)0xDB;  // █

  // Identificadores de los iconos dibujados (se usan con icon(IC_...)).
  enum IconId : uint8_t {
    IC_WARNING,   // triángulo de advertencia
    IC_DANGER,    // calavera
    IC_STABLE,    // carita sonriente (todo en orden)
    IC_COLD,      // copo de nieve
    IC_HEART,
    IC_TEMP,      // termómetro
    IC_DROP,      // gota
    IC_BELL,      // campana
    IC_CHECK,     // palomita
    IC_WIFI,
    IC_BATTERY,
    IC_FIRE,      // llama
    IC_COUNT
  };

  // address: dirección I2C (normalmente 0x3C, a veces 0x3D).
  // width/height: 128x64 o 128x32.
  // textScale: tamaño de letra (1 = pequeña, 2 = el doble...).
  // sda/scl: pines I2C del ESP32 (por defecto 21 y 22, los mismos del LCD y el RTC).
  OLEDI2C(uint8_t address = 0x3C, uint8_t width = 128, uint8_t height = 64,
          uint8_t textScale = 1, int sda = 21, int scl = 22, int8_t resetPin = -1)
    : Adafruit_SSD1306(width, height, &Wire, resetPin),
      _address(address), _scale(textScale < 1 ? 1 : textScale),
      _sda(sda), _scl(scl), _cols(0), _rows(0), _autoDisplay(true),
      _scrollActive(false), _scrollRow(0), _scrollPos(0),
      _scrollSpeed(300), _lastScroll(0),
      _loadActive(false), _loadRow(0), _loadFrame(0),
      _loadSpeed(120), _lastLoad(0) {
    memset(_custom, 0, sizeof(_custom));
    computeGrid();
  }

  // Llamar una vez en setup(): inicia I2C y la pantalla.
  // Devuelve false si no encuentra la OLED en la dirección indicada.
  bool begin() {
    Wire.begin(_sda, _scl);
    Wire.beginTransmission(_address);
    bool found = (Wire.endTransmission() == 0);

    // periphBegin = false: el I2C ya lo iniciamos arriba con los pines del ESP32
    if (!Adafruit_SSD1306::begin(SSD1306_SWITCHCAPVCC, _address, true, false)) return false;

    cp437(true);               // activa los caracteres especiales (°, flechas, bloque)
    setTextWrap(false);
    setTextColor(SSD1306_WHITE);
    setTextSize(_scale);
    clearDisplay();
    display();
    return found;
  }

  // Busca la OLED en 0x3C y 0x3D. Devuelve la dirección o 0 si no hay.
  uint8_t scanAddress() {
    const uint8_t candidates[2] = {0x3C, 0x3D};
    for (uint8_t i = 0; i < 2; i++) {
      Wire.beginTransmission(candidates[i]);
      if (Wire.endTransmission() == 0) return candidates[i];
    }
    return 0;
  }

  // Cambia el tamaño de letra (1-4). Cambia cuántas filas y columnas caben.
  void setTextScale(uint8_t scale) {
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    _scale = scale;
    setTextSize(_scale);
    computeGrid();
    clear();
  }

  // Limpia toda la pantalla y detiene animaciones.
  void clear() {
    _scrollActive = false;
    _loadActive = false;
    clearDisplay();
    flush();
  }

  // Controla si cada método envía los cambios a la pantalla al terminar (true por defecto).
  void setAutoDisplay(bool enabled) { _autoDisplay = enabled; }

  // Envía el contenido a la pantalla (útil con setAutoDisplay(false) o tras usar funciones gráficas).
  void refresh() { display(); }

  // =====================================================================
  //                              ICONOS
  // =====================================================================
  // Los iconos son caracteres que se concatenan con texto normal:
  //   oled.printLine(0, String("Temp ") + oled.iconCold() + " 5" + OLEDI2C::ICON_DEGREE + "C");

  char iconWarning() { return icon(IC_WARNING); }
  char iconDanger()  { return icon(IC_DANGER);  }
  char iconStable()  { return icon(IC_STABLE);  }
  char iconCold()    { return icon(IC_COLD);    }
  char iconHeart()   { return icon(IC_HEART);   }
  char iconTemp()    { return icon(IC_TEMP);    }
  char iconDrop()    { return icon(IC_DROP);    }
  char iconBell()    { return icon(IC_BELL);    }
  char iconCheck()   { return icon(IC_CHECK);   }
  char iconWifi()    { return icon(IC_WIFI);    }
  char iconBattery() { return icon(IC_BATTERY); }
  char iconFire()    { return icon(IC_FIRE);    }

  // Devuelve el carácter de cualquier icono.
  char icon(IconId id) {
    if (id >= IC_COUNT) return '?';
    return (char)(ICON_BASE + id);
  }

  // Define tu propio icono (5x8 píxeles) en una posición 1-4. Devuelve el carácter.
  // Cada byte es una fila (solo se usan los 5 bits de la derecha):
  //   uint8_t mi[8] = {0b00000, 0b01010, 0b01010, 0b00000, 0b10001, 0b01110, 0b00000, 0b00000};
  //   char c = oled.createIcon(1, mi);
  //   oled.printLine(0, String("Hola ") + c);
  // (Mismo formato de patrón que LCDI2C, por lo que sirve el mismo dibujo.)
  char createIcon(uint8_t slot, const uint8_t pattern[8]) {
    if (slot < 1 || slot > CUSTOM_SLOTS) return '?';
    for (uint8_t i = 0; i < 8; i++) _custom[slot - 1][i] = pattern[i];
    return (char)(ICON_BASE + IC_COUNT + slot - 1);
  }

  // ---------------- Estados listos para mostrar en una fila ----------------
  void showWarning(uint8_t row, const String& text) { printLine(row, String(iconWarning()) + " " + text); }
  void showDanger(uint8_t row, const String& text)  { printLine(row, String(iconDanger())  + " " + text); }
  void showStable(uint8_t row, const String& text)  { printLine(row, String(iconStable())  + " " + text); }
  void showCold(uint8_t row, const String& text)    { printLine(row, String(iconCold())    + " " + text); }

  // =====================================================================
  //                       ANIMACIONES DE CARGA
  // =====================================================================

  // Animación de inicio (BLOQUEANTE, úsala en setup() justo después de begin()):
  //   fila 0: spinner giratorio + texto   |   fila 1: barra de progreso que se llena
  // Al terminar muestra una palomita con doneText.
  void bootAnimation(const String& text = "Iniciando", unsigned long durationMs = 2500,
                     const String& doneText = "Listo") {
    if (durationMs == 0) durationMs = 1;
    clear();
    startLoading(0, text);

    bool hasBar = (_rows > 1);
    unsigned long start = millis();
    unsigned long elapsed;
    while ((elapsed = millis() - start) < durationMs) {
      update();
      if (hasBar) printBar(1, (uint8_t)((elapsed * 100UL) / durationMs));
      delay(20);
    }
    if (hasBar) printBar(1, 100);

    stopLoading(doneText);
    delay(600);  // para alcanzar a ver el mensaje final
  }

  // Spinner giratorio NO bloqueante en una fila (| / - \). Requiere update() en el loop().
  // Ej: oled.startLoading(1, "Conectando WiFi");
  void startLoading(uint8_t row, const String& text = "Cargando", unsigned long speedMs = 120) {
    if (row >= _rows) return;
    releaseRow(row);
    _loadActive = true;
    _loadRow = row;
    _loadFrame = 0;
    _loadSpeed = speedMs;
    _lastLoad = millis();

    int16_t y = row * cellH();
    fillRect(0, y, width(), cellH(), SSD1306_BLACK);
    drawGlyph(spinnerFrame(0), 0, y);
    if (_cols > 1) drawText(cellW(), y, String(" ") + text, _cols - 1);
    flush();
  }

  // Detiene el spinner. Con doneText muestra "palomita + texto"; sin él, borra la fila.
  void stopLoading(const String& doneText = "") {
    if (!_loadActive) return;
    uint8_t row = _loadRow;
    _loadActive = false;
    if (doneText.length() > 0) printLine(row, String(iconCheck()) + " " + doneText);
    else                       clearLine(row);
  }

  bool isLoading() const { return _loadActive; }

  // =====================================================================
  //                          ESCRITURA DE TEXTO
  // =====================================================================

  // Escribe texto en una fila completa (borra lo que había; recorta si es muy largo).
  void printLine(uint8_t row, const String& text) {
    if (row >= _rows) return;
    releaseRow(row);
    rawLine(row, text);
    flush();
  }

  // Escribe texto centrado en una fila.
  void printCentered(uint8_t row, const String& text) {
    if (row >= _rows) return;
    releaseRow(row);
    rawCentered(row, text);
    flush();
  }

  // Escribe texto en una posición exacta (columna, fila) sin borrar el resto de la fila.
  void printAt(uint8_t col, uint8_t row, const String& text) {
    if (row >= _rows || col >= _cols) return;
    drawText(col * cellW(), row * cellH(), text, _cols - col);
    flush();
  }

  // Borra una sola fila.
  void clearLine(uint8_t row) {
    if (row >= _rows) return;
    releaseRow(row);
    fillRect(0, row * cellH(), width(), cellH(), SSD1306_BLACK);
    flush();
  }

  // Limpia la pantalla y muestra dos líneas (la segunda es opcional).
  void showMessage(const String& line1, const String& line2 = "") {
    _scrollActive = false;
    _loadActive = false;
    clearDisplay();
    rawLine(0, line1);
    if (_rows > 1) rawLine(1, line2);
    flush();
  }

  // ---------------- Marquesina (texto desplazable, no bloqueante) ----------------

  // Desplaza un texto largo en una fila. speedMs = ms entre cada desplazamiento.
  // Requiere llamar a update() en el loop(). Si el texto cabe, solo lo muestra.
  void scrollText(uint8_t row, const String& text, unsigned long speedMs = 200) {
    if (row >= _rows) return;
    if (text.length() <= _cols) {
      printLine(row, text);
      return;
    }
    releaseRow(row);
    _scrollText = text + "    ";   // espacio entre vueltas
    _scrollRow = row;
    _scrollPos = 0;
    _scrollSpeed = speedMs;
    _scrollActive = true;
    _lastScroll = millis();
    renderScroll();
    flush();
  }

  void stopScroll() { _scrollActive = false; }

  // Llamar en cada vuelta del loop(): anima la marquesina y el spinner de carga.
  void update() {
    unsigned long now = millis();
    bool changed = false;

    if (_scrollActive && now - _lastScroll >= _scrollSpeed) {
      _lastScroll = now;
      _scrollPos = (_scrollPos + 1) % _scrollText.length();
      renderScroll();
      changed = true;
    }

    if (_loadActive && now - _lastLoad >= _loadSpeed) {
      _lastLoad = now;
      _loadFrame = (_loadFrame + 1) % 4;
      drawGlyph(spinnerFrame(_loadFrame), 0, _loadRow * cellH());
      changed = true;
    }

    if (changed) flush();
  }

  // ---------------- Barra de progreso ----------------

  // Dibuja una barra (0-100 %) con borde en una fila, con precisión de píxel.
  // startCol y widthCols (en columnas de texto) permiten dejar espacio para texto.
  void printBar(uint8_t row, uint8_t percent, uint8_t startCol = 0, uint8_t widthCols = 0) {
    if (row >= _rows || startCol >= _cols) return;
    if (widthCols == 0 || startCol + widthCols > _cols) widthCols = _cols - startCol;
    if (percent > 100) percent = 100;

    int16_t x = startCol * cellW();
    int16_t y = row * cellH();
    int16_t w = widthCols * cellW();
    int16_t h = cellH();

    fillRect(x, y, w, h, SSD1306_BLACK);
    drawRect(x, y, w, h, SSD1306_WHITE);
    int16_t fill = (int32_t)(w - 4) * percent / 100;
    if (fill > 0) fillRect(x + 2, y + 2, fill, h - 4, SSD1306_WHITE);
    flush();
  }

  // ---------------- Pantalla ----------------

  // Enciende o apaga la pantalla (equivale a la luz de fondo del LCD).
  void setBacklight(bool on) {
    ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
  }

  // Brillo de 0 a 255.
  void setContrast(uint8_t value) {
    ssd1306_command(SSD1306_SETCONTRAST);
    ssd1306_command(value);
  }

  uint8_t columns() const { return _cols; }
  uint8_t lines()   const { return _rows; }

private:
  static const uint8_t ICON_BASE    = 0x80;  // los iconos usan los códigos 0x80 en adelante
  static const uint8_t CUSTOM_SLOTS = 4;
  static const uint8_t ICON_SLOTS   = IC_COUNT + CUSTOM_SLOTS;

  uint8_t _address, _scale;
  int _sda, _scl;
  uint8_t _cols, _rows;
  bool _autoDisplay;

  uint8_t _custom[CUSTOM_SLOTS][8];

  // Estado de la marquesina
  bool _scrollActive;
  uint8_t _scrollRow;
  uint16_t _scrollPos;
  unsigned long _scrollSpeed, _lastScroll;
  String _scrollText;

  // Estado del spinner de carga
  bool _loadActive;
  uint8_t _loadRow, _loadFrame;
  unsigned long _loadSpeed, _lastLoad;

  int16_t cellW() const { return 6 * _scale; }  // ancho de un carácter (5 px + 1 de espacio)
  int16_t cellH() const { return 8 * _scale; }  // alto de un carácter

  void computeGrid() {
    _cols = WIDTH / (6 * _scale);
    _rows = HEIGHT / (8 * _scale);
  }

  void flush() {
    if (_autoDisplay) display();
  }

  // Dibujos de los iconos (5x8 píxeles). Mismo orden que IconId.
  const uint8_t* patternOf(uint8_t index) const {
    static const uint8_t icons[IC_COUNT][8] = {
      {0b00100, 0b01010, 0b01010, 0b10101, 0b10101, 0b10001, 0b11111, 0b00000},  // advertencia
      {0b01110, 0b11111, 0b10101, 0b11111, 0b01110, 0b01010, 0b01110, 0b00000},  // peligro (calavera)
      {0b00000, 0b01010, 0b01010, 0b00000, 0b10001, 0b01110, 0b00000, 0b00000},  // estable (sonrisa)
      {0b00100, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00100, 0b00000},  // frío (copo)
      {0b00000, 0b01010, 0b11111, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000},  // corazón
      {0b00100, 0b01010, 0b01010, 0b01010, 0b01110, 0b11111, 0b11111, 0b01110},  // termómetro
      {0b00100, 0b00100, 0b01010, 0b01010, 0b10001, 0b10001, 0b10001, 0b01110},  // gota
      {0b00100, 0b01110, 0b01110, 0b01110, 0b11111, 0b00000, 0b00100, 0b00000},  // campana
      {0b00000, 0b00001, 0b00011, 0b10110, 0b11100, 0b01000, 0b00000, 0b00000},  // palomita
      {0b00000, 0b01110, 0b10001, 0b00100, 0b01010, 0b00000, 0b00100, 0b00000},  // wifi
      {0b01110, 0b11011, 0b10001, 0b10001, 0b11111, 0b11111, 0b11111, 0b11111},  // batería
      {0b00100, 0b01100, 0b01110, 0b01111, 0b11111, 0b11111, 0b11111, 0b01110}   // llama
    };
    if (index < IC_COUNT) return icons[index];
    return _custom[index - IC_COUNT];
  }

  // Cuatro cuadros del spinner: | / - \ (se dibujan porque la fuente no trae todos)
  static const uint8_t* spinnerFrame(uint8_t frame) {
    static const uint8_t frames[4][8] = {
      {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},  // |
      {0b00001, 0b00010, 0b00010, 0b00100, 0b00100, 0b01000, 0b01000, 0b10000},  // /
      {0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b00000, 0b00000, 0b00000},  // -
      {0b10000, 0b01000, 0b01000, 0b00100, 0b00100, 0b00010, 0b00010, 0b00001}   // \ .
    };
    return frames[frame & 3];
  }

  // Dibuja un icono/glifo de 5x8 dentro de una celda de texto (con fondo negro).
  void drawGlyph(const uint8_t* pattern, int16_t x, int16_t y) {
    fillRect(x, y, cellW(), cellH(), SSD1306_BLACK);
    for (uint8_t r = 0; r < 8; r++) {
      for (uint8_t c = 0; c < 5; c++) {
        if (pattern[r] & (1 << (4 - c))) {
          if (_scale == 1) drawPixel(x + c, y + r, SSD1306_WHITE);
          else             fillRect(x + c * _scale, y + r * _scale, _scale, _scale, SSD1306_WHITE);
        }
      }
    }
  }

  // Dibuja texto (letras e iconos mezclados) desde la posición (x, y) en píxeles.
  void drawText(int16_t x, int16_t y, const String& s, uint8_t maxChars) {
    uint16_t n = s.length();
    if (n > maxChars) n = maxChars;
    for (uint16_t i = 0; i < n; i++) {
      uint8_t c = (uint8_t)s[i];
      if (c >= ICON_BASE && c < ICON_BASE + ICON_SLOTS) {
        drawGlyph(patternOf(c - ICON_BASE), x, y);
      } else {
        drawChar(x, y, c, SSD1306_WHITE, SSD1306_BLACK, _scale);
      }
      x += cellW();
    }
  }

  // Versiones sin envío a pantalla (para combinar varias y enviar una sola vez).
  void rawLine(uint8_t row, const String& text) {
    int16_t y = row * cellH();
    fillRect(0, y, width(), cellH(), SSD1306_BLACK);
    drawText(0, y, text, _cols);
  }

  void rawCentered(uint8_t row, const String& text) {
    String t = text.substring(0, _cols);
    uint8_t left = (_cols - t.length()) / 2;
    int16_t y = row * cellH();
    fillRect(0, y, width(), cellH(), SSD1306_BLACK);
    drawText(left * cellW(), y, t, _cols);
  }

  // Detiene marquesina/spinner si estaban usando esa fila.
  void releaseRow(uint8_t row) {
    if (_scrollActive && _scrollRow == row) _scrollActive = false;
    if (_loadActive && _loadRow == row)     _loadActive = false;
  }

  void renderScroll() {
    uint16_t len = _scrollText.length();
    String window = "";
    for (uint8_t i = 0; i < _cols; i++) {
      window += _scrollText[(_scrollPos + i) % len];
    }
    int16_t y = _scrollRow * cellH();
    fillRect(0, y, width(), cellH(), SSD1306_BLACK);
    drawText(0, y, window, _cols);
  }
};

#endif