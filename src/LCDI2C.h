#ifndef LCDI2C_H
#define LCDI2C_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // Librería "LiquidCrystal I2C" (Frank de Brabander)

// Clase para LCD con módulo I2C (16x2 por defecto) en ESP32, todo en un solo .h.
// HEREDA de LiquidCrystal_I2C, por lo que conservas todas sus funciones originales:
//   clear(), home(), setCursor(col,row), print(), write(), createChar(),
//   backlight(), noBacklight(), display(), noDisplay(), cursor(), blink(),
//   scrollDisplayLeft/Right(), etc.
// y se agregan: líneas completas, texto centrado, marquesina, barras de progreso,
// iconos (advertencia, peligro, estable, frío...) y animaciones de carga.
class LCDI2C : public LiquidCrystal_I2C {
public:
  // ---------------- Caracteres de fábrica (ROM del LCD, modelo A00) ----------------
  static const char ICON_DEGREE      = (char)0xDF;  // °
  static const char ICON_ARROW_RIGHT = (char)0x7E;  // →
  static const char ICON_ARROW_LEFT  = (char)0x7F;  // ←
  static const char ICON_BLOCK       = (char)0xFF;  // █

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
    IC_COUNT
  };

  // address: dirección I2C del módulo (normalmente 0x27 o 0x3F).
  // sda/scl: pines I2C del ESP32 (por defecto 21 y 22).
  LCDI2C(uint8_t address = 0x27, uint8_t cols = 16, uint8_t rows = 2,
         int sda = 21, int scl = 22)
    : LiquidCrystal_I2C(address, cols, rows),
      _address(address), _cols(cols), _rows(rows), _sda(sda), _scl(scl),
      _tick(0),
      _scrollActive(false), _scrollRow(0), _scrollPos(0),
      _scrollSpeed(300), _lastScroll(0),
      _loadActive(false), _loadRow(0), _loadFrame(0),
      _loadSpeed(120), _lastLoad(0) {
    resetIcons();
  }

  // Llamar una vez en setup(): inicia I2C, el LCD y enciende la luz de fondo.
  // Devuelve false si no encuentra el LCD en la dirección indicada.
  bool begin() {
    Wire.begin(_sda, _scl);
    Wire.beginTransmission(_address);
    bool found = (Wire.endTransmission() == 0);

    init();
    backlight();
    resetIcons();
    clear();
    return found;
  }

  // Busca el primer dispositivo I2C entre 0x20 y 0x3F (rango típico de los LCD).
  // Devuelve su dirección, o 0 si no encontró ninguno.
  uint8_t scanAddress() {
    for (uint8_t addr = 0x20; addr <= 0x3F; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) return addr;
    }
    return 0;
  }

  // =====================================================================
  //                              ICONOS
  // =====================================================================
  // El LCD solo guarda 8 dibujos a la vez. La clase carga cada icono la primera
  // vez que lo pides y, si hay más de 7 distintos, reemplaza el menos usado.
  // (Si tienes más de 7 iconos distintos visibles a la vez, el más viejo cambiará.)
  //
  // Uso: se concatenan con texto:
  //   lcd.printLine(0, String("Temp ") + lcd.iconCold() + " 5" + LCDI2C::ICON_DEGREE + "C");

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

  // Devuelve el carácter de cualquier icono (lo carga en el LCD si hace falta).
  char icon(IconId id) {
    if (id >= IC_COUNT) return '?';

    // ¿Ya está cargado?
    for (uint8_t s = 1; s <= 7; s++) {
      if (_slotIcon[s] == id) {
        _slotUse[s] = ++_tick;
        return (char)s;
      }
    }

    // Buscar un espacio libre o, si no hay, el menos usado (sin tocar los personalizados)
    int8_t target = -1;
    uint32_t oldest = 0xFFFFFFFFUL;
    for (uint8_t s = 1; s <= 7; s++) {
      if (_slotIcon[s] == SLOT_FREE) { target = s; break; }
      if (_slotIcon[s] != SLOT_CUSTOM && _slotUse[s] < oldest) {
        oldest = _slotUse[s];
        target = s;
      }
    }
    if (target < 0) return '?';  // todos los espacios están ocupados por iconos personalizados

    writeIconPattern(target, patternOf(id));
    _slotIcon[target] = id;
    _slotUse[target] = ++_tick;
    return (char)target;
  }

  // Define tu propio icono (5x8 píxeles) en una posición 1-7 y lo deja fijo
  // (no será reemplazado automáticamente). Devuelve el carácter para concatenar.
  //   uint8_t mi[8] = {0b00000, 0b01010, 0b01010, 0b00000, 0b10001, 0b01110, 0b00000, 0b00000};
  //   char c = lcd.createIcon(1, mi);
  //   lcd.printLine(0, String("Hola ") + c);
  // (No se llama createChar para no ocultar la función original heredada.)
  char createIcon(uint8_t slot, const uint8_t pattern[8]) {
    if (slot < 1 || slot > 7) return '?';
    writeIconPattern(slot, pattern);
    _slotIcon[slot] = SLOT_CUSTOM;
    return (char)slot;
  }

  // Libera todos los espacios de iconos (los personalizados también).
  void resetIcons() {
    for (uint8_t s = 0; s < 8; s++) {
      _slotIcon[s] = SLOT_FREE;
      _slotUse[s] = 0;
    }
    _tick = 0;
  }

  // ---------------- Estados listos para mostrar en una línea ----------------
  // Escriben "icono + texto" en la fila indicada.
  void showWarning(uint8_t row, const String& text) { printLine(row, String(iconWarning()) + " " + text); }
  void showDanger(uint8_t row, const String& text)  { printLine(row, String(iconDanger())  + " " + text); }
  void showStable(uint8_t row, const String& text)  { printLine(row, String(iconStable())  + " " + text); }
  void showCold(uint8_t row, const String& text)    { printLine(row, String(iconCold())    + " " + text); }

  // =====================================================================
  //                       ANIMACIONES DE CARGA
  // =====================================================================

  // Animación de inicio (BLOQUEANTE, úsala en setup() justo después de begin()):
  //   línea 0: spinner giratorio + texto   |   línea 1: barra de progreso que se llena
  // Al terminar muestra una palomita con doneText.
  void bootAnimation(const String& text = "Iniciando", unsigned long durationMs = 2500,
                     const String& doneText = "Listo") {
    if (durationMs == 0) durationMs = 1;
    clear();
    _scrollActive = false;
    startLoading(0, text);

    bool hasBar = (_rows > 1);
    if (hasBar) {
      setCursor(0, 1);
      write('[');
      setCursor(_cols - 1, 1);
      write(']');
    }

    unsigned long start = millis();
    unsigned long elapsed;
    while ((elapsed = millis() - start) < durationMs) {
      update();
      if (hasBar) printBar(1, (uint8_t)((elapsed * 100UL) / durationMs), 1, _cols - 2);
      delay(20);
    }
    if (hasBar) printBar(1, 100, 1, _cols - 2);

    stopLoading(doneText);
    delay(600);  // para alcanzar a ver el mensaje final
  }

  // Spinner giratorio NO bloqueante en una línea (| / - \). Requiere update() en el loop().
  // Ej: lcd.startLoading(1, "Conectando WiFi");
  void startLoading(uint8_t row, const String& text = "Cargando", unsigned long speedMs = 120) {
    if (row >= _rows) return;
    releaseRow(row);
    _loadActive = true;
    _loadRow = row;
    _loadFrame = 0;
    _loadSpeed = speedMs;
    _lastLoad = millis();

    writeIconPattern(0, spinnerFrame(0));  // el espacio 0 está reservado para el spinner
    setCursor(0, row);
    write((uint8_t)0);
    print(fitWidth(String(" ") + text, _cols - 1));
  }

  // Detiene el spinner. Con doneText muestra "palomita + texto"; sin él, borra la línea.
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

  // Escribe texto en una línea completa (borra lo que había; recorta si es muy largo).
  void printLine(uint8_t row, const String& text) {
    if (row >= _rows) return;
    releaseRow(row);
    setCursor(0, row);
    print(fitWidth(text, _cols));
  }

  // Escribe texto centrado en una línea.
  void printCentered(uint8_t row, const String& text) {
    if (row >= _rows) return;
    releaseRow(row);
    String t = text.substring(0, _cols);
    uint8_t left = (_cols - t.length()) / 2;
    String line = "";
    for (uint8_t i = 0; i < left; i++) line += ' ';
    line += t;
    setCursor(0, row);
    print(fitWidth(line, _cols));
  }

  // Escribe texto en una posición exacta sin borrar el resto de la línea.
  void printAt(uint8_t col, uint8_t row, const String& text) {
    if (row >= _rows || col >= _cols) return;
    setCursor(col, row);
    print(text.substring(0, _cols - col));
  }

  // Borra una sola línea.
  void clearLine(uint8_t row) {
    if (row >= _rows) return;
    releaseRow(row);
    setCursor(0, row);
    print(fitWidth("", _cols));
  }

  // Limpia la pantalla y muestra dos líneas (la segunda es opcional).
  void showMessage(const String& line1, const String& line2 = "") {
    _scrollActive = false;
    _loadActive = false;
    clear();
    printLine(0, line1);
    if (_rows > 1) printLine(1, line2);
  }

  // ---------------- Marquesina (texto desplazable, no bloqueante) ----------------

  // Desplaza un texto largo en una línea. speedMs = ms entre cada desplazamiento.
  // Requiere llamar a update() en el loop(). Si el texto cabe, solo lo muestra.
  void scrollText(uint8_t row, const String& text, unsigned long speedMs = 300) {
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
  }

  void stopScroll() { _scrollActive = false; }

  // Llamar en cada vuelta del loop(): anima la marquesina y el spinner de carga.
  void update() {
    unsigned long now = millis();

    if (_scrollActive && now - _lastScroll >= _scrollSpeed) {
      _lastScroll = now;
      _scrollPos = (_scrollPos + 1) % _scrollText.length();
      renderScroll();
    }

    if (_loadActive && now - _lastLoad >= _loadSpeed) {
      _lastLoad = now;
      _loadFrame = (_loadFrame + 1) % 4;
      writeIconPattern(0, spinnerFrame(_loadFrame));  // el carácter 0 se redibuja solo
    }
  }

  // ---------------- Barra de progreso ----------------

  // Dibuja una barra (0-100 %) en una línea usando el carácter de bloque lleno.
  // startCol y width permiten dejar espacio para texto (ej. un "75%" al final).
  void printBar(uint8_t row, uint8_t percent, uint8_t startCol = 0, uint8_t width = 0) {
    if (row >= _rows || startCol >= _cols) return;
    if (width == 0 || startCol + width > _cols) width = _cols - startCol;
    if (percent > 100) percent = 100;

    uint8_t filled = (uint16_t)percent * width / 100;
    setCursor(startCol, row);
    for (uint8_t i = 0; i < width; i++) {
      if (i < filled) write((uint8_t)255);  // bloque lleno
      else            write(' ');
    }
  }

  // Enciende o apaga la luz de fondo.
  void setBacklight(bool on) {
    if (on) backlight();
    else    noBacklight();
  }

  uint8_t columns() const { return _cols; }
  uint8_t lines()   const { return _rows; }

private:
  static const uint8_t SLOT_FREE   = 0xFF;
  static const uint8_t SLOT_CUSTOM = 0xFE;

  uint8_t _address, _cols, _rows;
  int _sda, _scl;

  // Estado de los espacios de iconos del LCD (0 = reservado al spinner)
  uint8_t _slotIcon[8];
  uint32_t _slotUse[8];
  uint32_t _tick;

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

  // Dibujos de los iconos (5x8 píxeles). Mismo orden que IconId.
  static const uint8_t* patternOf(IconId id) {
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
      {0b01110, 0b11011, 0b10001, 0b10001, 0b11111, 0b11111, 0b11111, 0b11111}   // batería
    };
    return icons[id];
  }

  // Cuatro cuadros del spinner: | / - \ (el carácter '\' no existe en la ROM, por eso se dibuja)
  static const uint8_t* spinnerFrame(uint8_t frame) {
    static const uint8_t frames[4][8] = {
      {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},  // |
      {0b00001, 0b00010, 0b00010, 0b00100, 0b00100, 0b01000, 0b01000, 0b10000},  // /
      {0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b00000, 0b00000, 0b00000},  // -
      {0b10000, 0b01000, 0b01000, 0b00100, 0b00100, 0b00010, 0b00010, 0b00001}   // \ .
    };
    return frames[frame & 3];
  }

  void writeIconPattern(uint8_t slot, const uint8_t* pattern) {
    uint8_t buf[8];
    for (uint8_t i = 0; i < 8; i++) buf[i] = pattern[i];
    createChar(slot, buf);
    setCursor(0, 0);  // vuelve a la memoria de pantalla tras definir el carácter
  }

  // Ajusta el texto a un ancho (recorta o rellena con espacios).
  String fitWidth(const String& text, uint8_t width) {
    String t = text.substring(0, width);
    while (t.length() < width) t += ' ';
    return t;
  }

  // Detiene marquesina/spinner si estaban usando esa fila.
  void releaseRow(uint8_t row) {
    if (_scrollActive && _scrollRow == row) _scrollActive = false;
    if (_loadActive && _loadRow == row)     _loadActive = false;
  }

  void renderScroll() {
    setCursor(0, _scrollRow);
    uint16_t len = _scrollText.length();
    for (uint8_t i = 0; i < _cols; i++) {
      write((uint8_t)_scrollText[(_scrollPos + i) % len]);
    }
  }
};

#endif
