#ifndef SDMODULE_H
#define SDMODULE_H

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include "vfs_api.h"

// Bus SPI a usar (VSPI en ESP32 clásico; FSPI en S2/S3/C3)
#if defined(VSPI)
  #define SDMODULE_SPI_BUS VSPI
#else
  #define SDMODULE_SPI_BUS FSPI
#endif

// Clase para módulo microSD por SPI en ESP32 (todo en un solo .h).
// HEREDA de SDFS (la clase de la librería SD del ESP32, la misma de la que sale
// el objeto global "SD"), por lo que conservas todas sus funciones originales:
//   open(), exists(), remove(), rename(), mkdir(), rmdir(),
//   cardType(), cardSize(), totalBytes(), usedBytes()
// y se agregan métodos para leer/escribir archivos de forma sencilla.
class SDModule : public SDFS {
public:
  // Pines por defecto = VSPI estándar del ESP32:
  //   CS = 5, SCK = 19, MISO = 18, MOSI = 23
  // frequency: velocidad SPI en Hz (4 MHz es seguro; puedes subir a 20000000).
  SDModule(uint8_t csPin = 5, int8_t sckPin = 19, int8_t misoPin = 18,
           int8_t mosiPin = 23, uint32_t frequency = 4000000)
    : SDFS(FSImplPtr(new VFSImpl())),
      _cs(csPin), _sck(sckPin), _miso(misoPin), _mosi(mosiPin),
      _freq(frequency), _spi(SDMODULE_SPI_BUS), _mounted(false) {}

  // Llamar una vez en setup(): inicia el SPI y monta la tarjeta (FAT16/FAT32).
  // Devuelve false si no hay tarjeta o falla el montaje.
  bool begin() {
    _spi.begin(_sck, _miso, _mosi, _cs);
    _mounted = SDFS::begin(_cs, _spi, _freq);
    return _mounted;
  }

  // Desmonta la tarjeta.
  void end() {
    SDFS::end();
    _spi.end();
    _mounted = false;
  }

  bool isMounted() const { return _mounted; }

  // ---------------- Lectura / escritura sencilla ----------------

  // Crea (o sobrescribe) un archivo con el texto indicado.
  bool writeFile(const char* path, const String& text) {
    return writeInternal(path, text, FILE_WRITE);
  }

  // Agrega texto al final del archivo (lo crea si no existe).
  bool appendFile(const char* path, const String& text) {
    return writeInternal(path, text, FILE_APPEND);
  }

  // Agrega una línea (con salto de línea) al final del archivo. Ideal para logs/CSV.
  bool appendLine(const char* path, const String& line) {
    return writeInternal(path, line + "\n", FILE_APPEND);
  }

  // Lee todo el archivo y lo devuelve como String ("" si no existe o está vacío).
  String readFile(const char* path) {
    if (!_mounted) return "";
    File f = open(path, FILE_READ);
    if (!f || f.isDirectory()) return "";
    String content = f.readString();
    f.close();
    return content;
  }

  // ---------------- Información de la tarjeta ----------------

  const char* cardTypeName() {
    switch (cardType()) {
      case CARD_MMC:  return "MMC";
      case CARD_SD:   return "SDSC";
      case CARD_SDHC: return "SDHC";
      case CARD_NONE: return "SIN TARJETA";
      default:        return "DESCONOCIDA";
    }
  }

  uint64_t totalMB() { return cardSize()  / (1024ULL * 1024ULL); }  // capacidad de la tarjeta
  uint64_t usedMB()  { return usedBytes() / (1024ULL * 1024ULL); }
  uint64_t freeMB()  { return (totalBytes() - usedBytes()) / (1024ULL * 1024ULL); }

  // Imprime tipo, capacidad y espacio usado/libre.
  void printInfo(Print& out = Serial) {
    if (!_mounted) {
      out.println("SD: no montada");
      return;
    }
    out.printf("SD tipo: %s | Capacidad: %llu MB | Usado: %llu MB | Libre: %llu MB\n",
               cardTypeName(), totalMB(), usedMB(), freeMB());
  }

  // Lista los archivos de un directorio (nombre y tamaño en bytes).
  void listFiles(const char* dir = "/", Print& out = Serial) {
    if (!_mounted) return;
    File root = open(dir);
    if (!root || !root.isDirectory()) {
      out.printf("No se pudo abrir el directorio: %s\n", dir);
      return;
    }
    File entry = root.openNextFile();
    while (entry) {
      if (entry.isDirectory()) out.printf("[DIR]  %s\n", entry.name());
      else                     out.printf("       %s  (%u bytes)\n", entry.name(), (unsigned)entry.size());
      entry = root.openNextFile();
    }
  }

private:
  uint8_t _cs;
  int8_t _sck, _miso, _mosi;
  uint32_t _freq;
  SPIClass _spi;
  bool _mounted;

  bool writeInternal(const char* path, const String& text, const char* mode) {
    if (!_mounted) return false;
    File f = open(path, mode);
    if (!f) return false;
    size_t written = f.print(text);
    f.close();
    return written == text.length();
  }
};

#endif
