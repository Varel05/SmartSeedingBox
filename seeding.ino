#define BLYNK_TEMPLATE_ID "TMPL6WSqikMb8"
#define BLYNK_TEMPLATE_NAME "seeding"
#define BLYNK_AUTH_TOKEN "Vx5gqVfaaQLZYu2wjlGwbebuzQ-NxAaW"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "DHTesp.h"

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Yoshiyuki";
char pass[] = "loreksa029";

// ------------------- PIN SETUP -------------------
#define DHTPIN 22
#define SOIL_PIN 35
#define LDR_PIN 34
#define RELAY_FAN 25
#define RELAY_LIGHT 26
#define RELAY_MIST 33

DHTesp dht;

// ------------------- BATAS SENSOR (ON) -------------------
float TEMP_ON_THRESHOLD = 32.0;
float HUMI_ON_THRESHOLD = 75.0;
float SOIL_ON_THRESHOLD = 40.0;
float LIGHT_ON_THRESHOLD = 30.0;

// ------------------- HISTERESIS (OFF) -------------------
float TEMP_OFF_THRESHOLD = 30.0;
float HUMI_OFF_THRESHOLD = 70.0;
float SOIL_OFF_THRESHOLD = 45.0;
float LIGHT_OFF_THRESHOLD = 40.0;

// ------------------- STATUS MODE -------------------
int autoMode = 1; // 1 = otomatis, 0 = manual

// ------------------- MANUAL CONTROL -------------------
int fanManual = 0;
int lightManual = 0;
int mistManual = 0;

// ------------------- STATUS RELAY OTOMATIS -------------------
bool fanAutoState = false;
bool lightAutoState = false;
bool mistAutoState = false;

// ------------------- DEBOUNCE -------------------
unsigned long lastFanChange = 0;
unsigned long lastLightChange = 0;
unsigned long lastMistChange = 0;
const unsigned long debounceDelay = 300; // ms

// ------------------- BLYNK PINS -------------------
#define VPIN_MODE V0
#define VPIN_FAN V1
#define VPIN_LIGHT V2
#define VPIN_MIST V3
#define VPIN_TEMP V4
#define VPIN_HUMI V5
#define VPIN_SOIL V6
#define VPIN_LIGHTLVL V7

// ----------------------------------------------------
// 🧩 Fungsi bantu untuk relay aktif LOW
// ----------------------------------------------------
void relayWrite(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH); // LOW = ON, HIGH = OFF
}

// ----------------------------------------------------
BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_MODE);
  Serial.println("🔄 Sinkronisasi status awal dari server Blynk...");
}

// ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  dht.setup(DHTPIN, DHTesp::DHT11);

  // Pastikan relay OFF dulu sebelum set pinMode
  digitalWrite(RELAY_FAN, HIGH);
  digitalWrite(RELAY_LIGHT, HIGH);
  digitalWrite(RELAY_MIST, HIGH);

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_MIST, OUTPUT);

  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);

  Serial.println("🌱 Sistem Monitoring & Kontrol Otomatis Tanaman Aktif");
  Serial.println("Mode awal: Otomatis (Auto)");

  Blynk.virtualWrite(VPIN_MODE, autoMode);
  Blynk.virtualWrite(VPIN_FAN, fanManual);
  Blynk.virtualWrite(VPIN_LIGHT, lightManual);
  Blynk.virtualWrite(VPIN_MIST, mistManual);
}

// ----------------------------------------------------
void loop() {
  Blynk.run();

  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate >= 500) {
    lastSensorUpdate = millis();

    TempAndHumidity data = dht.getTempAndHumidity();
    float temperature = data.temperature;
    float humidity = data.humidity;

    if (!isnan(temperature) && !isnan(humidity)) {
      Blynk.virtualWrite(VPIN_TEMP, temperature);
      Blynk.virtualWrite(VPIN_HUMI, humidity);
    }

    int soilValue = analogRead(SOIL_PIN);
    int ldrValue = analogRead(LDR_PIN);

    float soilPercent = map(soilValue, 0, 4095, 0, 100);
    float lightPercent = map(ldrValue, 0, 4095, 100, 0);

    Blynk.virtualWrite(VPIN_SOIL, soilPercent);
    Blynk.virtualWrite(VPIN_LIGHTLVL, lightPercent);

    // ------------------- MODE OTOMATIS -------------------
    if (autoMode == 1) {
      // FAN
      if (!fanAutoState && (temperature > TEMP_ON_THRESHOLD || humidity > HUMI_ON_THRESHOLD))
        fanAutoState = true;
      else if (fanAutoState && (temperature < TEMP_OFF_THRESHOLD && humidity < HUMI_OFF_THRESHOLD))
        fanAutoState = false;

      // MIST
      if (!mistAutoState && soilPercent < SOIL_ON_THRESHOLD)
        mistAutoState = true;
      else if (mistAutoState && soilPercent > SOIL_OFF_THRESHOLD)
        mistAutoState = false;

      // LIGHT
      if (!lightAutoState && lightPercent < LIGHT_ON_THRESHOLD)
        lightAutoState = true;
      else if (lightAutoState && lightPercent > LIGHT_OFF_THRESHOLD)
        lightAutoState = false;

      relayWrite(RELAY_FAN, fanAutoState);
      relayWrite(RELAY_MIST, mistAutoState);
      relayWrite(RELAY_LIGHT, lightAutoState);
    } 
    else {
      // ------------------- MODE MANUAL -------------------
      relayWrite(RELAY_FAN, fanManual);
      relayWrite(RELAY_LIGHT, lightManual);
      relayWrite(RELAY_MIST, mistManual);
    }
  }
}

// ----------------------------------------------------
BLYNK_WRITE(VPIN_MODE) {
  int newMode = param.asInt();
  if (newMode == autoMode) return;

  autoMode = newMode;
  if (autoMode == 1) {
    Serial.println("🔧 Beralih ke Mode Otomatis");
  } else {
    Serial.println("⚙️ Beralih ke Mode Manual (anti reset aktif)");
  }
  Blynk.virtualWrite(VPIN_MODE, autoMode);
}

// ----------------------------------------------------
BLYNK_WRITE(VPIN_FAN) {
  if (autoMode == 0) {
    unsigned long now = millis();
    if (now - lastFanChange > debounceDelay) {
      fanManual = param.asInt();
      relayWrite(RELAY_FAN, fanManual);
      Serial.printf("Fan Manual: %s\n", fanManual ? "ON" : "OFF");
      lastFanChange = now;
    }
  } else {
    Blynk.virtualWrite(VPIN_FAN, 0);
    Serial.println("❌ Fan ditolak: Sistem dalam Mode Otomatis.");
  }
}

// ----------------------------------------------------
BLYNK_WRITE(VPIN_LIGHT) {
  if (autoMode == 0) {
    unsigned long now = millis();
    if (now - lastLightChange > debounceDelay) {
      lightManual = param.asInt();
      relayWrite(RELAY_LIGHT, lightManual);
      Serial.printf("Lampu Manual: %s\n", lightManual ? "ON" : "OFF");
      lastLightChange = now;
    }
  } else {
    Blynk.virtualWrite(VPIN_LIGHT, 0);
    Serial.println("❌ Lampu ditolak: Sistem dalam Mode Otomatis.");
  }
}

// ----------------------------------------------------
BLYNK_WRITE(VPIN_MIST) {
  if (autoMode == 0) {
    unsigned long now = millis();
    if (now - lastMistChange > debounceDelay) {
      mistManual = param.asInt();
      relayWrite(RELAY_MIST, mistManual);
      Serial.printf("Mist Manual: %s\n", mistManual ? "ON" : "OFF");
      lastMistChange = now;
    }
  } else {
    Blynk.virtualWrite(VPIN_MIST, 0);
    Serial.println("❌ Mist ditolak: Sistem dalam Mode Otomatis.");
  }
}
