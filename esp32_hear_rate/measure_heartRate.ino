#include <Arduino.h>
#include <Wire.h>

#include "MAX30105.h"
#include "heartRate.h"

// ============================================================
// ESP-WROOM-32 I2C pins
// ============================================================
#define SDA_PIN 21
#define SCL_PIN 22

MAX30105 sensor;

// ============================================================
// SETTINGS
// ============================================================

const long FINGER_THRESHOLD = 10000;

const float MIN_BPM = 40.0;
const float MAX_BPM = 180.0;

const byte RATE_SIZE = 4;

byte rates[RATE_SIZE] = {0};
byte rateIndex = 0;
byte validRates = 0;

unsigned long lastBeat = 0;
unsigned long fingerStart = 0;
unsigned long lastSerial = 0;

float instantBPM = 0.0;
float averageBPM = 0.0;

int shownBPM = 0;

bool fingerPresent = false;

// ============================================================
// RESET HEART RATE
// ============================================================

void resetHeartRate()
{
  lastBeat = 0;

  instantBPM = 0.0;
  averageBPM = 0.0;
  shownBPM = 0;

  rateIndex = 0;
  validRates = 0;

  for (byte i = 0; i < RATE_SIZE; i++)
  {
    rates[i] = 0;
  }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 HEART RATE MONITOR");
  Serial.println(" MAX30102 - SERIAL ONLY");
  Serial.println("================================");

  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // ==========================================================
  // Initialize MAX30102
  // ==========================================================

  if (!sensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("ERROR: MAX30102 not found!");
    Serial.println("Check wiring.");

    while (1)
    {
      delay(1000);
    }
  }

  // ==========================================================
  // Sensor configuration
  // ==========================================================

  byte ledBrightness = 15;
  byte sampleAverage = 4;
  byte ledMode = 2;       // RED + IR
  int sampleRate = 100;   // 100 Hz
  int pulseWidth = 411;
  int adcRange = 16384;

  sensor.setup(
    ledBrightness,
    sampleAverage,
    ledMode,
    sampleRate,
    pulseWidth,
    adcRange
  );

  sensor.setPulseAmplitudeRed(0x05);
  sensor.setPulseAmplitudeGreen(0);

  resetHeartRate();

  Serial.println("MAX30102 initialized successfully.");
  Serial.println();
  Serial.println("Place finger gently on sensor.");
  Serial.println();
}


void loop()
{
  sensor.check();

  while (sensor.available())
  {
    long irValue = sensor.getFIFOIR();

    sensor.nextSample();

    // ========================================================
    // Finger detection
    // ========================================================

    if (irValue < FINGER_THRESHOLD)
    {
      if (fingerPresent)
      {
        Serial.println();
        Serial.println("Finger removed.");

        resetHeartRate();
      }

      fingerPresent = false;

      continue;
    }

    // ========================================================
    // New finger detecttion
    // ========================================================

    if (!fingerPresent)
    {
      fingerPresent = true;
      fingerStart = millis();

      resetHeartRate();

      Serial.println();
      Serial.println("Finger detected.");
      Serial.println("Stabilizing signal...");
    }

    if (millis() - fingerStart < 1500)
    {
      continue;
    }

    // ========================================================
    // Detect heartbeat
    // ========================================================

    if (checkForBeat(irValue))
    {
      unsigned long now = millis();

      if (lastBeat == 0)
      {
        lastBeat = now;

        Serial.println("First stable heartbeat detected.");

        continue;
      }

      unsigned long delta = now - lastBeat;
      lastBeat = now;

      if (delta == 0)
        continue;

      // Calculate instantaneous BPM
      instantBPM = 60000.0 / (float)delta;


      if (instantBPM >= MIN_BPM &&
          instantBPM <= MAX_BPM)
      {
        rates[rateIndex] = round(instantBPM);

        rateIndex++;

        if (rateIndex >= RATE_SIZE)
          rateIndex = 0;

        if (validRates < RATE_SIZE)
          validRates++;

        // ====================================================
        // Calculate moving average
        // ====================================================

        int total = 0;

        for (byte i = 0; i < validRates; i++)
        {
          total += rates[i];
        }

        averageBPM = (float)total / validRates;

        shownBPM = round(averageBPM);

        // ====================================================
        // Print result
        // ====================================================

        Serial.print("HEARTBEAT | Instant BPM: ");
        Serial.print(instantBPM, 1);

        Serial.print(" | Average BPM: ");
        Serial.println(shownBPM);
      }
      else
      {
        Serial.print("Rejected beat: ");
        Serial.print(instantBPM, 1);
        Serial.println(" BPM");
      }
    }

    if (millis() - lastSerial >= 500)
    {
      lastSerial = millis();

      Serial.print("IR=");
      Serial.print(irValue);

      if (shownBPM > 0)
      {
        Serial.print(" | HR=");
        Serial.print(shownBPM);
        Serial.println(" BPM");
      }
      else
      {
        Serial.println(" | Searching for heartbeat...");
      }
    }
  }

  delay(1);
}