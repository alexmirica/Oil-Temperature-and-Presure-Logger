// -----------------------------------------------------------------------------
// main.cpp
// Oil Pressure, Oil Temperature, Battery Voltage Logger and Serial Monitor
// -----------------------------------------------------------------------------
// Author: Tech Art Electronics - Dipl. Ing. Alexandru-Bogdan Mirica
//
// Description:
//  This program reads oil pressure, oil temperature, and battery voltage
//  from an automotive system and logs the data to an SD card in CSV format.
//  It also provides a serial interface for monitoring and controlling the
//  logging process.
//
// Responsibilities:
//  - Read analog inputs for oil pressure, oil temperature, and battery voltage
//  - Log data to an SD card in CSV format
//  - Provide a serial interface for monitoring and controlling the logging
//  - Display current readings and status on an LCD
//  - Handle user input via buttons for starting/stopping logging
//  - Implement a command parser for serial commands
//  - Ensure proper error handling for SD card operations
// Usage:
//  - Connect the appropriate sensors to the specified analog pins
//  - Connect an SD card module to the specified SPI pins
//  - Connect an LCD to the specified digital pins
//  - Use the serial monitor to send commands and view logs
// Limitations:
//  - The program assumes specific hardware connections and may not work with
//    different configurations without modification.
//  - The program is designed for non-commercial use and may require licensing
//    for commercial applications.
// Notes:
//  - Ensure that the SD card is formatted correctly and has sufficient space
//  - The program uses the Steinhart-Hart equation for temperature conversion
//  - The program includes a simple moving average filter for temperature
//  readings
//  - The program includes error handling for SD card initialization and file
//  operations
// -----------------------------------------------------------------------------
// Revision History / To Do:
//   - v0.1: Initial release with basic logging and serial monitoring
//   functionality
//   - v0.2: Added Steinhart-Hart temperature conversion and moving average
//   filter
//   - v0.3: Improved error handling and user interface on LCD
//   - v0.4: Added command parser for serial commands and file management
//   - v0.5: Optimized code for performance and reduced memory usage
//   - v0.6: Added support for additional sensors and improved logging format
//   - v0.7: Implemented user feedback on LCD for logging status and errors
//   - v0.8: Enhanced serial command parser with additional commands and help
//   menu
//   - v0.9: Finalized code for production use with comprehensive testing and
//   validation
//
// Integrated with:
//   - Arduino core libraries
//   - LiquidCrystal library for LCD display
//   - SD library for SD card operations
//   - SPI library for SPI communication with SD card
//   - Math library for mathematical operations
//   - Ctype library for character handling
//   - String library for string manipulation
//
// License:
//   This source code is provided strictly for **non-commercial use only**.
//   You may study, modify, and use this code for personal or educational
//   purposes. **Commercial use, redistribution, or incorporation into any
//   commercial product requires prior written consent from
//   Tech Art Electronics - Dipl. Ing. Alexandru-Bogdan Mirica.
//
//   DO NOT COPY, duplicate, or distribute any part of this code for
//   commercial purposes without explicit permission.
//
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <SD.h>
#include <SPI.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

// -----------------------------------------------------------------------------
// LCD
// -----------------------------------------------------------------------------

constexpr uint8_t LCD_RS = 8;
constexpr uint8_t LCD_EN = 9;
constexpr uint8_t LCD_D4 = 4;
constexpr uint8_t LCD_D5 = 5;
constexpr uint8_t LCD_D6 = 6;
constexpr uint8_t LCD_D7 = 7;

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// -----------------------------------------------------------------------------
// ADC PINS
// -----------------------------------------------------------------------------

constexpr uint8_t PIN_KEYS = A0;
constexpr uint8_t PIN_PRESSURE = A1;
constexpr uint8_t PIN_TEMP = A2;
constexpr uint8_t PIN_VBATT = A3;

// -----------------------------------------------------------------------------
// SD
// -----------------------------------------------------------------------------

constexpr uint8_t SD_CS = 10;

bool sdOK = false;
bool SDinitialized = false;
bool logging = false;
File logFile;

// -----------------------------------------------------------------------------
// PRESSURE SENSOR
// -----------------------------------------------------------------------------

const float MaxOilPress = 6.0f;
const float ZeroOffset = 0.5f;
const float FullScale = 4.5f;

// -----------------------------------------------------------------------------
// BATTERY DIVIDER
// -----------------------------------------------------------------------------

#define VbattRu 22.0f
#define VbattRd 10.0f

// -----------------------------------------------------------------------------
// NTC
// -----------------------------------------------------------------------------

#define TempCounts 5

const float Vref = 5.0f;
const int ADC_MAX = 1023;
const float Rs = 200.0f;

struct NTCPoint {
  float T;
  float R;
};

float rBuf[TempCounts];
int rIndex = 0;
bool rFilled = false;

// Steinhart-Hart coefficients for the NTC thermistor
float ntcToTemp(float R) {
  const float A = 1.92540359e-3f;
  const float B = 1.63461544e-4f;
  const float C = 1.16831585e-6f;

  float lnR = log(R);

  float invT = A + B * lnR + C * lnR * lnR * lnR;

  return (1.0f / invT) - 273.15f;
}

// -----------------------------------------------------------------------------
// TIMEKEEPING
// -----------------------------------------------------------------------------

uint32_t startMillis;

// -----------------------------------------------------------------------------
// LOGGING
// -----------------------------------------------------------------------------

char serialBuf[48];
uint8_t serialPos = 0;
bool SerialLog = false;

// -----------------------------------------------------------------------------
// PROTOTYPES
// -----------------------------------------------------------------------------

float readPressureBar();
float readBatteryVoltage();
float readTemperatureC();

float readRntc(int adcRaw);
float filterR(float R);
float ntcToTemp(float R);

void updateLCD();
void updateSerial();

int getKeyPress();
void handleLoggingButton();

bool startLogging();
void stopLogging();
void writeLogLine();

void handleSerial();
void processCommand(char *cmd);
void printHelp();

void listDirectory();
void printFile(const char *name);
void deleteFile(const char *name);
void deleteAllCSV();
void handleSDInitialization();

// -----------------------------------------------------------------------------
// SD INITIALIZATION
// -----------------------------------------------------------------------------

void handleSDInitialization() {
  while (!SDinitialized) {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("SD Card Error");

    lcd.setCursor(0, 1);
    lcd.print("B=Ignore S=Retry");

    int key = getKeyPress();

    // SELECT = retry
    if (key == 5) {
      delay(250);

      sdOK = SD.begin(SD_CS);

      if (sdOK) {
        SDinitialized = true;
        lcd.clear();
        delay(300);
        return;
      }
    }

    // BACK = ignore SD and continue
    if (key == 2) {
      delay(250);

      SDinitialized = false;
      logging = false;

      lcd.clear();
      return;
    }

    delay(20);
  }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  lcd.clear();

  startMillis = millis();

  sdOK = SD.begin(SD_CS);
  SDinitialized = sdOK;

  if (!SDinitialized)
    handleSDInitialization();
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------

void loop() {
  static uint32_t lastDisplay = 0;
  static uint32_t lastSerial = 0;
  static uint32_t lastLog = 0;

  handleLoggingButton();

  handleSerial();

  if (millis() - lastDisplay >= 200) {
    lastDisplay = millis();
    updateLCD();
  }

  if (millis() - lastSerial >= 1000) {
    lastSerial = millis();
    updateSerial();
  }

  if (logging && millis() - lastLog >= 1000) {
    lastLog = millis();
    writeLogLine();
  }
}

// -----------------------------------------------------------------------------
// KEYPAD
// -----------------------------------------------------------------------------

int getKeyPress() {
  int x = analogRead(PIN_KEYS);

  if (x < 100)
    return 1;
  if (x < 200)
    return 3;
  if (x < 400)
    return 4;
  if (x < 600)
    return 2;
  if (x < 800)
    return 5;

  return 0;
}

void handleLoggingButton() {
  static bool keyHeld = false;
  static uint32_t lastAction = 0;

  int key = getKeyPress();

  if (key == 5) {
    // New press
    if (!keyHeld && millis() - lastAction >= 250) {
      keyHeld = true;
      lastAction = millis();

      if (logging)
        stopLogging();
      else if (SDinitialized)
        startLogging();
    }
  } else {
    // Key released
    keyHeld = false;
  }
}

// -----------------------------------------------------------------------------
// PRESSURE
// -----------------------------------------------------------------------------

const float ADC_TO_VOLTS = 5.0f / ADC_MAX;

float readPressureBar() {
  float volts = analogRead(PIN_PRESSURE) * ADC_TO_VOLTS;

  return ((volts - ZeroOffset) / (FullScale - ZeroOffset)) * MaxOilPress;
}

// -----------------------------------------------------------------------------
// BATTERY
// -----------------------------------------------------------------------------

float readBatteryVoltage() {
  float vadc = analogRead(PIN_VBATT) * ADC_TO_VOLTS;

  return vadc * ((VbattRu + VbattRd) / VbattRd);
}

// -----------------------------------------------------------------------------
// NTC
// -----------------------------------------------------------------------------

float readRntc(int adcRaw) {
  float v = adcRaw * (Vref / ADC_MAX);

  if (v >= Vref - 0.01f)
    v = Vref - 0.01f;

  return Rs * v / (Vref - v);
}

float filterR(float R) {
  rBuf[rIndex] = R;

  rIndex++;

  if (rIndex >= TempCounts) {
    rIndex = 0;
    rFilled = true;
  }

  int count = rFilled ? TempCounts : rIndex;

  float sum = 0;

  for (int i = 0; i < count; i++)
    sum += rBuf[i];

  return sum / count;
}

float readTemperatureC() {
  return ntcToTemp(filterR(readRntc(analogRead(PIN_TEMP))));
}

// -----------------------------------------------------------------------------
// LCD
// -----------------------------------------------------------------------------

void updateLCD() {
  uint32_t elapsed = (millis() - startMillis) / 1000UL;

  uint8_t hh = elapsed / 3600;
  uint8_t mm = (elapsed % 3600) / 60;
  uint8_t ss = elapsed % 60;

  float vbatt = readBatteryVoltage();
  float temp = readTemperatureC();

  float pressureV = analogRead(PIN_PRESSURE) * ADC_TO_VOLTS;

  char buf[8];

  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);

  lcd.print(hh);
  lcd.print(":");

  if (mm < 10)
    lcd.print('0');
  lcd.print(mm);
  lcd.print(":");

  if (ss < 10)
    lcd.print('0');
  lcd.print(ss);

  lcd.print(" ");

  dtostrf(vbatt, 4, 1, buf);
  lcd.print(buf);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);

  if (pressureV < 0.5f)
    lcd.print("UV");
  else if (pressureV > 4.5f)
    lcd.print("OV");
  else {
    dtostrf(readPressureBar(), 4, 2, buf);
    lcd.print(buf);
    lcd.print("bar");
  }

  lcd.setCursor(9, 1);
  lcd.print((int)temp);
  lcd.print("C");

  lcd.setCursor(15, 1);
  lcd.print(logging ? 'R' : ' ');
}

// -----------------------------------------------------------------------------
// SERIAL
// -----------------------------------------------------------------------------

void updateSerial() {
  if (!SerialLog)
    return;

  uint32_t elapsed = (millis() - startMillis) / 1000UL;

  Serial.print(elapsed);
  Serial.print("s ");

  Serial.print(readBatteryVoltage(), 1);
  Serial.print("V ");

  float pressureV = analogRead(PIN_PRESSURE) * ADC_TO_VOLTS;

  if (pressureV < 0.5f)
    Serial.print("UV");
  else if (pressureV > 4.5f)
    Serial.print("OV");
  else {
    Serial.print(readPressureBar(), 2);
    Serial.print("bar");
  }

  Serial.print(" ");
  Serial.print(readTemperatureC(), 1);
  Serial.println("C");
}

// -----------------------------------------------------------------------------
// SERIAL COMMAND PARSER
// -----------------------------------------------------------------------------

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r')
      continue;

    if (c == '\n') {
      serialBuf[serialPos] = '\0';

      if (serialPos)
        processCommand(serialBuf);

      serialPos = 0;
      continue;
    }

    if (serialPos < sizeof(serialBuf) - 1)
      serialBuf[serialPos++] = c;
  }
}

void processCommand(char *cmd) {
  while (*cmd == ' ')
    cmd++;

  if (!strcasecmp(cmd, "dir")) {
    listDirectory();
    return;
  }

  if (!strcasecmp(cmd, "help")) {
    printHelp();
    return;
  }

  if (!strcasecmp(cmd, "logon")) {
    SerialLog = true;
    Serial.println(F("Serial logging ON."));
    return;
  }

  if (!strcasecmp(cmd, "logoff")) {
    SerialLog = false;
    Serial.println(F("Serial logging OFF."));
    return;
  }

  if (!strncasecmp(cmd, "list ", 5)) {
    printFile(cmd + 5);
    return;
  }

  if (!strncasecmp(cmd, "del ", 4)) {
    char *name = cmd + 4;

    if (!strcasecmp(name, "*.csv"))
      deleteAllCSV();
    else
      deleteFile(name);

    return;
  }

  printHelp();
}

void printFile(const char *name) {
  if (!SDinitialized) {
    Serial.println(F("SD Card Error"));
    return;
  }

  File f = SD.open(name, FILE_READ);

  if (!f) {
    Serial.print(name);
    Serial.println(F(" not found."));
    return;
  }

  while (f.available())
    Serial.write(f.read());

  f.close();
}

void printHelp() {
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("dir              - list SD files"));
  Serial.println(F("list file.ext    - output file"));
  Serial.println(F("del file.ext     - delete file"));
  Serial.println(F("del *.csv        - delete all CSV logs"));
  Serial.println(F("logon            - enable serial logging"));
  Serial.println(F("logoff           - disable serial logging"));
  Serial.println(F("help             - this help"));
  Serial.println();
}

void listDirectory() {
  if (!SDinitialized) {
    Serial.println(F("SD Card Error"));
    return;
  }

  File root = SD.open("/");

  Serial.println(F("Directory:"));

  while (true) {
    File entry = root.openNextFile();

    if (!entry)
      break;

    Serial.print(entry.name());
    Serial.print(F("  "));
    Serial.print(entry.size());
    Serial.println(F(" bytes"));

    entry.close();
  }

  root.close();
}

void deleteFile(const char *name) {
  if (!SDinitialized) {
    Serial.println(F("SD Card Error"));
    return;
  }

  if (logging && logFile && !strcasecmp(name, logFile.name())) {
    stopLogging();
  }

  if (SD.exists(name)) {
    SD.remove(name);

    Serial.print(F("File "));
    Serial.print(name);
    Serial.println(F(" deleted."));
  } else {
    Serial.print(F("File "));
    Serial.print(name);
    Serial.println(F(" not found."));
  }
}

void deleteAllCSV() {
  if (!SDinitialized) {
    Serial.println(F("SD Card Error"));
    return;
  }

  File root = SD.open("/");

  int deleted = 0;

  while (true) {
    File entry = root.openNextFile();

    if (!entry)
      break;

    char name[13];
    strncpy(name, entry.name(), sizeof(name));
    name[12] = '\0';

    entry.close();

    int len = strlen(name);

    if (len >= 4 && !strcasecmp(name + len - 4, ".CSV")) {
      if (logging && logFile && !strcasecmp(name, logFile.name()))
        stopLogging();

      if (SD.remove(name)) {
        Serial.print(F("Deleted "));
        Serial.println(name);
        deleted++;
      }
    }
  }

  root.close();

  Serial.print(deleted);
  Serial.println(F(" CSV files deleted."));
}

// -----------------------------------------------------------------------------
// SD LOGGING
// -----------------------------------------------------------------------------

bool startLogging() {
  if (!SDinitialized)
    return false;

  char filename[13];

  for (int i = 1; i <= 999; i++) {
    sprintf(filename, "LOG%03d.CSV", i);

    if (!SD.exists(filename)) {
      logFile = SD.open(filename, FILE_WRITE);

      if (!logFile)
        return false;

      logFile.println("Time,OilP,OilT,Vbatt");
      logFile.flush();

      logging = true;

      return true;
    }
  }

  return false;
}

void stopLogging() {
  if (logFile)
    logFile.close();

  logging = false;
}

void writeLogLine() {
  if (!logFile)
    return;

  uint32_t t = (millis() - startMillis) / 1000UL;

  float pressureV = analogRead(PIN_PRESSURE) * ADC_TO_VOLTS;

  logFile.print(t);
  logFile.print(",");

  if (pressureV < 0.5f)
    logFile.print("UV");
  else if (pressureV > 4.5f)
    logFile.print("OV");
  else
    logFile.print(readPressureBar(), 2);

  logFile.print(",");
  logFile.print(readTemperatureC(), 1);
  logFile.print(",");
  logFile.println(readBatteryVoltage(), 1);

  logFile.flush();
}

// main.cpp end