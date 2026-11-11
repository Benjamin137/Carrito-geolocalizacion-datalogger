#ifndef RTCCLOCK_H
#define RTCCLOCK_H

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <RTClib.h>   // Librería "RTClib" de Adafruit

// Clase para RTC DS1307 en ESP32 (todo en un solo .h).
// HEREDA de RTC_DS1307 (RTClib), por lo que conservas sus funciones originales:
//   now(), adjust(), isrunning(), etc.
// y se agregan: actualización de fecha/hora, múltiples formatos de salida
// y mapeo de nombres de meses y días en español e inglés.
//
class RTCClock : public RTC_DS1307 {
public:
  enum Language { LANG_ES, LANG_EN };

  // sda/scl: pines I2C del ESP32 (por defecto 21 y 22, los mismos del LCD I2C).
  RTCClock(int sda = 21, int scl = 22, Language lang = LANG_ES)
    : _sda(sda), _scl(scl), _lang(lang) {}

  // Llamar una vez en setup(). Devuelve false si no encuentra el RTC.
  // Si el RTC perdió la hora (pila agotada o primer uso) y autoSetIfLostPower = true,
  // lo ajusta con la fecha/hora de compilación del programa.
  bool begin(bool autoSetIfLostPower = true) {
    Wire.begin(_sda, _scl);
    if (!RTC_DS1307::begin(&Wire)) return false;
    if (autoSetIfLostPower && !isrunning()) setFromCompileTime();
    return true;
  }

  void setLanguage(Language lang) { _lang = lang; }
  Language getLanguage() const { return _lang; }

  // =====================================================================
  //                     ACTUALIZAR FECHA Y HORA DEL RTC
  // =====================================================================

  // Ajusta fecha y hora completas. Devuelve false si algún valor es inválido.
  bool setDateTime(uint16_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t minute, uint8_t second) {
    if (year < 2000 || year > 2099) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > daysInMonth(year, month)) return false;
    if (hour > 23 || minute > 59 || second > 59) return false;
    adjust(DateTime(year, month, day, hour, minute, second));
    return true;
  }

  // Cambia solo la fecha (conserva la hora actual).
  bool setDate(uint16_t year, uint8_t month, uint8_t day) {
    DateTime t = now();
    return setDateTime(year, month, day, t.hour(), t.minute(), t.second());
  }

  // Cambia solo la hora (conserva la fecha actual).
  bool setTime(uint8_t hour, uint8_t minute, uint8_t second = 0) {
    DateTime t = now();
    return setDateTime(t.year(), t.month(), t.day(), hour, minute, second);
  }

  // Ajusta con la fecha y hora en que se compiló el programa.
  // (Queda atrasado unos segundos/minutos respecto a la hora real.)
  void setFromCompileTime() {
    adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Ajusta desde un texto con formato "AAAA-MM-DD HH:MM:SS", ej. "2026-10-08 14:30:00".
  bool setFromString(const char* text) {
    int y, mo, d, h, mi, s;
    if (sscanf(text, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return false;
    return setDateTime(y, mo, d, h, mi, s);
  }

  // Ajusta desde un timestamp Unix (segundos desde 1970).
  void setFromUnix(uint32_t timestamp) {
    adjust(DateTime(timestamp));
  }

  // Sincroniza con un servidor NTP por internet (requiere WiFi YA conectado).
  //   gmtOffsetSec: zona horaria en segundos (México centro = -6 * 3600 = -21600)
  //   daylightOffsetSec: horario de verano en segundos (0 si no aplica)
  bool syncWithNTP(long gmtOffsetSec, int daylightOffsetSec = 0,
                   const char* server = "pool.ntp.org", uint32_t timeoutMs = 10000) {
    configTime(gmtOffsetSec, daylightOffsetSec, server);
    struct tm t;
    if (!getLocalTime(&t, timeoutMs)) return false;
    return setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                       t.tm_hour, t.tm_min, t.tm_sec);
  }

  // =====================================================================
  //                         VALORES SUELTOS
  // =====================================================================

  uint16_t getYear()   { return now().year(); }
  uint8_t  getMonth()  { return now().month(); }
  uint8_t  getDay()    { return now().day(); }
  uint8_t  getHour()   { return now().hour(); }      // 0-23
  uint8_t  getMinute() { return now().minute(); }
  uint8_t  getSecond() { return now().second(); }
  uint8_t  getWeekday() { return now().dayOfTheWeek(); }  // 0 = domingo ... 6 = sábado
  uint32_t getUnix()   { return now().unixtime(); }

  // =====================================================================
  //                      NOMBRES DE MESES Y DÍAS
  // =====================================================================

  // Nombre del mes (1-12) en el idioma indicado. shortName = true -> abreviado ("Oct").
  // Nota: los nombres en español van sin acentos para que se vean bien en el LCD.
  static const char* monthName(uint8_t month, Language lang = LANG_ES, bool shortName = false) {
    static const char* const esLong[12] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                                           "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
    static const char* const esShort[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                                            "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    static const char* const enLong[12] = {"January", "February", "March", "April", "May", "June",
                                           "July", "August", "September", "October", "November", "December"};
    static const char* const enShort[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 1 || month > 12) return "?";
    if (lang == LANG_ES) return shortName ? esShort[month - 1] : esLong[month - 1];
    return shortName ? enShort[month - 1] : enLong[month - 1];
  }

  // Nombre del día de la semana (0 = domingo ... 6 = sábado).
  static const char* dayName(uint8_t weekday, Language lang = LANG_ES, bool shortName = false) {
    static const char* const esLong[7] = {"Domingo", "Lunes", "Martes", "Miercoles",
                                          "Jueves", "Viernes", "Sabado"};
    static const char* const esShort[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    static const char* const enLong[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                          "Thursday", "Friday", "Saturday"};
    static const char* const enShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (weekday > 6) return "?";
    if (lang == LANG_ES) return shortName ? esShort[weekday] : esLong[weekday];
    return shortName ? enShort[weekday] : enLong[weekday];
  }

  // Nombre del mes / día ACTUAL en el idioma configurado.
  String getMonthName(bool shortName = false) { return String(monthName(getMonth(), _lang, shortName)); }
  String getDayName(bool shortName = false)   { return String(dayName(getWeekday(), _lang, shortName)); }

  // Cantidad de días de un mes (considera años bisiestos).
  static uint8_t daysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return days[month - 1];
  }

  static bool isLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  }

  // =====================================================================
  //                         FORMATOS DE FECHA
  // =====================================================================

  String getDate() {        // 2026-10-08  (ISO)
    DateTime t = now();
    return String(t.year()) + "-" + pad2(t.month()) + "-" + pad2(t.day());
  }

  String getDateLatin() {   // 08/10/2026  (DD/MM/AAAA)
    DateTime t = now();
    return pad2(t.day()) + "/" + pad2(t.month()) + "/" + String(t.year());
  }

  String getDateUS() {      // 10/08/2026  (MM/DD/AAAA)
    DateTime t = now();
    return pad2(t.month()) + "/" + pad2(t.day()) + "/" + String(t.year());
  }

  String getDateShort() {   // 08/10/26
    DateTime t = now();
    return pad2(t.day()) + "/" + pad2(t.month()) + "/" + pad2(t.year() % 100);
  }

  // ES: "jueves, 8 de octubre de 2026"   EN: "Thursday, October 8, 2026"
  String getDateLong() {
    DateTime t = now();
    String d = String(dayName(t.dayOfTheWeek(), _lang));
    String m = String(monthName(t.month(), _lang));
    if (_lang == LANG_ES) {
      d.toLowerCase();
      m.toLowerCase();
      return d + ", " + String(t.day()) + " de " + m + " de " + String(t.year());
    }
    return d + ", " + m + " " + String(t.day()) + ", " + String(t.year());
  }

  // "Jue 08 Oct 2026"
  String getDateMedium() {
    DateTime t = now();
    return String(dayName(t.dayOfTheWeek(), _lang, true)) + " " + pad2(t.day()) + " " +
           String(monthName(t.month(), _lang, true)) + " " + String(t.year());
  }

  // =====================================================================
  //                         FORMATOS DE HORA
  // =====================================================================

  String getTime24() {      // 14:30:05
    DateTime t = now();
    return pad2(t.hour()) + ":" + pad2(t.minute()) + ":" + pad2(t.second());
  }

  String getTimeShort() {   // 14:30
    DateTime t = now();
    return pad2(t.hour()) + ":" + pad2(t.minute());
  }

  String getTime12() {      // 02:30:05 PM
    DateTime t = now();
    return pad2(hour12(t.hour())) + ":" + pad2(t.minute()) + ":" + pad2(t.second()) +
           (t.hour() < 12 ? " AM" : " PM");
  }

  String getTime12Short() { // 02:30 PM
    DateTime t = now();
    return pad2(hour12(t.hour())) + ":" + pad2(t.minute()) + (t.hour() < 12 ? " AM" : " PM");
  }

  // =====================================================================
  //                       FECHA Y HORA COMBINADAS
  // =====================================================================

  String getDateTime() {          // 2026-10-08 14:30:05
    return getDate() + " " + getTime24();
  }

  String getDateTimeLatin() {     // 08/10/2026 14:30:05
    return getDateLatin() + " " + getTime24();
  }

  String getISO8601() {           // 2026-10-08T14:30:05
    return getDate() + "T" + getTime24();
  }

  // Ideal para nombres de archivo en la SD: 20261008_143005
  String getTimestamp() {
    DateTime t = now();
    return String(t.year()) + pad2(t.month()) + pad2(t.day()) + "_" +
           pad2(t.hour()) + pad2(t.minute()) + pad2(t.second());
  }

  // =====================================================================
  //                       FORMATO PERSONALIZADO
  // =====================================================================
  // Tokens (distinguen mayúsculas/minúsculas):
  //   YYYY año (2026)      YY año corto (26)
  //   MMMM mes completo    MMM mes abreviado    MM mes numérico (10)
  //   DDDD día completo    DDD día abreviado    DD día del mes (08)
  //   hh hora 24 h (14)    HH hora 12 h (02)    mm minutos    ss segundos
  //   AP "AM" o "PM"
  // El resto de caracteres se copian tal cual. Los nombres salen en el idioma configurado.
  // Ej: format("DDDD DD de MMMM, hh:mm")  ->  "Jueves 08 de Octubre, 14:30"
  String format(const String& pattern) {
    DateTime t = now();
    String out = "";
    size_t i = 0;
    while (i < pattern.length()) {
      if      (pattern.startsWith("YYYY", i)) { out += String(t.year());                              i += 4; }
      else if (pattern.startsWith("YY", i))   { out += pad2(t.year() % 100);                          i += 2; }
      else if (pattern.startsWith("MMMM", i)) { out += monthName(t.month(), _lang, false);            i += 4; }
      else if (pattern.startsWith("MMM", i))  { out += monthName(t.month(), _lang, true);             i += 3; }
      else if (pattern.startsWith("MM", i))   { out += pad2(t.month());                               i += 2; }
      else if (pattern.startsWith("DDDD", i)) { out += dayName(t.dayOfTheWeek(), _lang, false);       i += 4; }
      else if (pattern.startsWith("DDD", i))  { out += dayName(t.dayOfTheWeek(), _lang, true);        i += 3; }
      else if (pattern.startsWith("DD", i))   { out += pad2(t.day());                                 i += 2; }
      else if (pattern.startsWith("hh", i))   { out += pad2(t.hour());                                i += 2; }
      else if (pattern.startsWith("HH", i))   { out += pad2(hour12(t.hour()));                        i += 2; }
      else if (pattern.startsWith("mm", i))   { out += pad2(t.minute());                              i += 2; }
      else if (pattern.startsWith("ss", i))   { out += pad2(t.second());                              i += 2; }
      else if (pattern.startsWith("AP", i))   { out += (t.hour() < 12 ? "AM" : "PM");                 i += 2; }
      else                                    { out += pattern[i];                                    i += 1; }
    }
    return out;
  }

private:
  int _sda, _scl;
  Language _lang;

  static String pad2(uint16_t value) {
    String s(value);
    if (value < 10) s = "0" + s;
    return s;
  }

  static uint8_t hour12(uint8_t hour24) {
    uint8_t h = hour24 % 12;
    return (h == 0) ? 12 : h;
  }
};

#endif
