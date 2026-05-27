const byte DOOR_SENSOR_PIN = 2;
const byte LCD_SDA_PIN = A2;
const byte LCD_SCL_PIN = A3;
const byte GREEN_LAMP_RELAY_PIN = 8;
const byte RED_ALARM_RELAY_PIN = 9;
const byte BUZZER_PIN = 10;

const unsigned long ALARM_DELAY_MS = 5000;
const unsigned long DOOR_DEBOUNCE_MS = 300;
const unsigned long LCD_REFRESH_MS = 1000;
const unsigned long LCD_RESCAN_MS = 3000;

const byte RELAY_ON = LOW;
const byte RELAY_OFF = HIGH;

const byte GREEN_LAMP_ON = LOW;         // Green turns off when D8 goes HIGH.
const byte GREEN_LAMP_OFF = HIGH;
const byte RED_LAMP_ON = RELAY_OFF;     // Red tower lamp is acting like NC.
const byte RED_LAMP_OFF = RELAY_ON;
const byte BUZZER_OFF = HIGH;           // This buzzer/module turns on when D10 goes LOW.

const byte DOOR_CLOSED_SENSOR_STATE = LOW;

const byte LCD_RS = 0x01;
const byte LCD_ENABLE = 0x04;
const byte LCD_BACKLIGHT = 0x08;

bool stableDoorOpen = false;
bool lastStableDoorOpen = false;
bool lastRawDoorOpen = false;
bool alarmActive = false;
bool lcdDetected = false;

byte lcdAddress = 0;

unsigned long doorOpenedTime = 0;
unsigned long doorRawChangedTime = 0;
unsigned long lastDebugTime = 0;
unsigned long lastDisplayRefreshTime = 0;
unsigned long lastLcdRescanTime = 0;

int lastDisplayScreen = -1;

void setup() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, GREEN_LAMP_OFF);
  digitalWrite(RED_ALARM_RELAY_PIN, RED_LAMP_OFF);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(GREEN_LAMP_RELAY_PIN, OUTPUT);
  pinMode(RED_ALARM_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lastRawDoorOpen = readDoorOpenRaw();
  stableDoorOpen = lastRawDoorOpen;
  lastStableDoorOpen = stableDoorOpen;
  doorRawChangedTime = millis();

  Serial.begin(9600);
  softI2cRelease(LCD_SDA_PIN);
  softI2cRelease(LCD_SCL_PIN);

  byte detectedAddr = detectLcdAddress();
  lcdAddress = detectedAddr;
  lcdDetected = detectedAddr != 0;

  if (detectedAddr == 0) {
    Serial.println("No I2C LCD detected, using default 0x27");
    initializeLcd(0x27);
  } else {
    Serial.print("Using I2C device at 0x");
    if (detectedAddr < 16) Serial.print("0");
    Serial.println(detectedAddr, HEX);
    initializeLcd(detectedAddr);
  }

  lcdSetCursor(0, 0);
  lcdPrint("SYSTEM START");
  delay(2000);

  updateSystemState();
  updateDisplay(true);
}

void loop() {
  updateSystemState();
  maintainLcdConnection();
  updateDisplay(false);
  printDebugStatus();
}

void updateSystemState() {
  updateDoorStableState();

  if (!stableDoorOpen) {
    doorOpenedTime = 0;
    alarmActive = false;
    setDoorClosedOutputs();
    lastStableDoorOpen = stableDoorOpen;
    return;
  }

  if (!lastStableDoorOpen) {
    doorOpenedTime = millis();
    alarmActive = false;
  }

  unsigned long openDuration = millis() - doorOpenedTime;

  if (openDuration >= ALARM_DELAY_MS) {
    alarmActive = true;
    setAlarmOutputs();
  } else {
    alarmActive = false;
    setDoorOpenWaitingOutputs();
  }

  lastStableDoorOpen = stableDoorOpen;
}

bool readDoorOpenRaw() {
  return digitalRead(DOOR_SENSOR_PIN) != DOOR_CLOSED_SENSOR_STATE;
}

void updateDoorStableState() {
  bool currentRawDoorOpen = readDoorOpenRaw();

  if (currentRawDoorOpen != lastRawDoorOpen) {
    lastRawDoorOpen = currentRawDoorOpen;
    doorRawChangedTime = millis();
  }

  if (millis() - doorRawChangedTime >= DOOR_DEBOUNCE_MS) {
    stableDoorOpen = currentRawDoorOpen;
  }
}

void setDoorClosedOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, GREEN_LAMP_ON);
  digitalWrite(RED_ALARM_RELAY_PIN, RED_LAMP_OFF);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
}

void setDoorOpenWaitingOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, GREEN_LAMP_OFF);
  digitalWrite(RED_ALARM_RELAY_PIN, RED_LAMP_OFF);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
}

void setAlarmOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, GREEN_LAMP_OFF);
  digitalWrite(RED_ALARM_RELAY_PIN, RED_LAMP_ON);
  tone(BUZZER_PIN, 2000);
}

void updateDisplay(bool forceUpdate) {
  int currentScreen = stableDoorOpen ? 1 : 0;

  bool refreshDue = millis() - lastDisplayRefreshTime >= LCD_REFRESH_MS;

  if (!forceUpdate && !refreshDue && currentScreen == lastDisplayScreen) {
    return;
  }

  if (currentScreen == 0) {
    writeLcdLine(0, "DOOR CLOSE");
    writeLcdLine(1, "");
  } else {
    writeLcdLine(0, "DOOR OPEN");
    writeLcdLine(1, "");
  }

  lastDisplayScreen = currentScreen;
  lastDisplayRefreshTime = millis();
}

void writeLcdLine(byte row, const char* text) {
  lcdSetCursor(0, row);

  byte column = 0;
  while (text[column] != '\0' && column < 16) {
    lcdWrite(text[column]);
    column++;
  }

  while (column < 16) {
    lcdWrite(' ');
    column++;
  }
}

void initializeLcd(byte address) {
  lcdAddress = address;
  lcdInit();
  lastDisplayScreen = -1;
  lastDisplayRefreshTime = 0;
}

void maintainLcdConnection() {
  if (lcdDetected || millis() - lastLcdRescanTime < LCD_RESCAN_MS) {
    return;
  }

  lastLcdRescanTime = millis();

  byte detectedAddr = detectLcdAddress();
  if (detectedAddr == 0) {
    return;
  }

  lcdAddress = detectedAddr;
  lcdDetected = true;
  initializeLcd(lcdAddress);

  Serial.print("LCD reconnected at 0x");
  if (lcdAddress < 16) Serial.print("0");
  Serial.println(lcdAddress, HEX);
}

void printDebugStatus() {
  if (millis() - lastDebugTime < 1000) {
    return;
  }

  lastDebugTime = millis();

  Serial.print("STATUS = ");

  if (stableDoorOpen) {
    Serial.print("DOOR OPEN");
  } else {
    Serial.print("DOOR CLOSE");
  }

  Serial.print(" | D2 = ");
  Serial.print(digitalRead(DOOR_SENSOR_PIN));
  Serial.print(" | ALARM = ");
  if (alarmActive) {
    Serial.print("ON");
  } else {
    Serial.print("OFF");
  }

  Serial.print(" | GREEN_PIN = ");
  Serial.print(digitalRead(GREEN_LAMP_RELAY_PIN));

  Serial.print(" | RED_PIN = ");
  Serial.print(digitalRead(RED_ALARM_RELAY_PIN));

  Serial.print(" | SDA = ");
  Serial.print(digitalRead(LCD_SDA_PIN));
  Serial.print(" | SCL = ");
  Serial.print(digitalRead(LCD_SCL_PIN));

  Serial.print(" | LCD = ");
  if (lcdDetected) {
    Serial.print("0x");
    if (lcdAddress < 16) Serial.print("0");
    Serial.println(lcdAddress, HEX);
  } else {
    Serial.println("NO I2C");
  }
}

byte detectLcdAddress() {
  byte foundDevices = 0;
  bool found27 = false;
  bool found3F = false;
  byte firstFound = 0;

  Serial.println("Scanning software I2C on A2/A3...");

  for (byte address = 1; address < 127; address++) {
    if (softI2cProbe(address)) {
      Serial.print("I2C device found at 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      foundDevices++;
      if (firstFound == 0) firstFound = address;
      if (address == 0x27) found27 = true;
      if (address == 0x3F) found3F = true;
    }
  }

  if (foundDevices == 0) {
    Serial.println("No I2C device found");
  }

  if (found27) return 0x27;
  if (found3F) return 0x3F;

  return firstFound;
}

void lcdInit() {
  delay(50);

  lcdWrite4Bits(0x30);
  delay(5);
  lcdWrite4Bits(0x30);
  delayMicroseconds(150);
  lcdWrite4Bits(0x30);
  delayMicroseconds(150);
  lcdWrite4Bits(0x20);
  delayMicroseconds(150);

  lcdCommand(0x28);
  lcdCommand(0x08);
  lcdClear();
  lcdCommand(0x06);
  lcdCommand(0x0C);
}

void lcdClear() {
  lcdCommand(0x01);
  delay(2);
}

void lcdSetCursor(byte column, byte row) {
  const byte rowOffsets[] = {0x00, 0x40};
  if (row > 1) row = 1;
  lcdCommand(0x80 | (column + rowOffsets[row]));
}

void lcdPrint(const char* text) {
  while (*text != '\0') {
    lcdWrite(*text);
    text++;
  }
}

void lcdCommand(byte value) {
  lcdSend(value, 0);
}

void lcdWrite(byte value) {
  lcdSend(value, LCD_RS);
}

void lcdSend(byte value, byte mode) {
  lcdWrite4Bits((value & 0xF0) | mode);
  lcdWrite4Bits(((value << 4) & 0xF0) | mode);
}

void lcdWrite4Bits(byte value) {
  lcdExpanderWrite(value);
  lcdPulseEnable(value);
}

void lcdPulseEnable(byte value) {
  lcdExpanderWrite(value | LCD_ENABLE);
  delayMicroseconds(1);
  lcdExpanderWrite(value & ~LCD_ENABLE);
  delayMicroseconds(50);
}

bool lcdExpanderWrite(byte value) {
  softI2cStart();
  bool ok = softI2cWriteByte(lcdAddress << 1);
  ok = softI2cWriteByte(value | LCD_BACKLIGHT) && ok;
  softI2cStop();
  return ok;
}

bool softI2cProbe(byte address) {
  softI2cStart();
  bool ok = softI2cWriteByte(address << 1);
  softI2cStop();
  return ok;
}

void softI2cStart() {
  softI2cRelease(LCD_SDA_PIN);
  softI2cRelease(LCD_SCL_PIN);
  softI2cDelay();
  softI2cLow(LCD_SDA_PIN);
  softI2cDelay();
  softI2cLow(LCD_SCL_PIN);
}

void softI2cStop() {
  softI2cLow(LCD_SDA_PIN);
  softI2cDelay();
  softI2cRelease(LCD_SCL_PIN);
  softI2cDelay();
  softI2cRelease(LCD_SDA_PIN);
  softI2cDelay();
}

bool softI2cWriteByte(byte value) {
  for (byte mask = 0x80; mask != 0; mask >>= 1) {
    if (value & mask) {
      softI2cRelease(LCD_SDA_PIN);
    } else {
      softI2cLow(LCD_SDA_PIN);
    }

    softI2cDelay();
    softI2cRelease(LCD_SCL_PIN);
    softI2cDelay();
    softI2cLow(LCD_SCL_PIN);
  }

  softI2cRelease(LCD_SDA_PIN);
  softI2cDelay();
  softI2cRelease(LCD_SCL_PIN);
  softI2cDelay();
  bool acknowledged = digitalRead(LCD_SDA_PIN) == LOW;
  softI2cLow(LCD_SCL_PIN);
  softI2cDelay();

  return acknowledged;
}

void softI2cLow(byte pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

void softI2cRelease(byte pin) {
  pinMode(pin, INPUT_PULLUP);
}

void softI2cDelay() {
  delayMicroseconds(10);
}
