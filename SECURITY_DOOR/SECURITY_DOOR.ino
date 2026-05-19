#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const byte DOOR_SENSOR_PIN = 2;
const byte GREEN_LAMP_RELAY_PIN = 8;
const byte RED_ALARM_RELAY_PIN = 9;
const byte BUZZER_PIN = 10;

const unsigned long ALARM_DELAY_MS = 5000;

const byte RELAY_ON = LOW;
const byte RELAY_OFF = HIGH;

const byte DOOR_CLOSED_SENSOR_STATE = LOW;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool stableDoorOpen = false;
bool lastStableDoorOpen = false;
bool alarmActive = false;

unsigned long doorOpenedTime = 0;
unsigned long lastDebugTime = 0;

int lastDisplayScreen = -1;

void setup() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, RELAY_OFF);
  digitalWrite(RED_ALARM_RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(GREEN_LAMP_RELAY_PIN, OUTPUT);
  pinMode(RED_ALARM_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SYSTEM START");
  delay(2000);

  updateSystemState();
  updateDisplay(true);
}

void loop() {
  updateSystemState();
  updateDisplay(false);
  printDebugStatus();
}

void updateSystemState() {
  stableDoorOpen = digitalRead(DOOR_SENSOR_PIN) != DOOR_CLOSED_SENSOR_STATE;

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

void setDoorClosedOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, RELAY_ON);
  digitalWrite(RED_ALARM_RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);
}

void setDoorOpenWaitingOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, RELAY_ON);
  digitalWrite(RED_ALARM_RELAY_PIN, RELAY_OFF);
  digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);
}

void setAlarmOutputs() {
  digitalWrite(GREEN_LAMP_RELAY_PIN, RELAY_OFF);
  digitalWrite(RED_ALARM_RELAY_PIN, RELAY_ON);
  tone(BUZZER_PIN, 2000);
}

void updateDisplay(bool forceUpdate) {
  int currentScreen;

  if (stableDoorOpen) {
    currentScreen = 1;
  } else {
    currentScreen = 0;
  }

  if (!forceUpdate && currentScreen == lastDisplayScreen) {
    return;
  }

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("STATUS =");

  if (stableDoorOpen) {
    lcd.setCursor(0, 1);
    lcd.print("DR.OPEN");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("DR.CLOSE");
  }

  lastDisplayScreen = currentScreen;
}

void printDebugStatus() {
  if (millis() - lastDebugTime < 1000) {
    return;
  }

  lastDebugTime = millis();

  Serial.print("STATUS = ");

  if (stableDoorOpen) {
    Serial.print("DR.OPEN");
  } else {
    Serial.print("DR.CLOSE");
  }

  Serial.print(" | D2 = ");
  Serial.println(digitalRead(DOOR_SENSOR_PIN));
}
