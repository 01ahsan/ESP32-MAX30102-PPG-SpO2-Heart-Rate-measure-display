#include <Arduino.h>
#include <Wire.h>

#include "MAX30105.h"
#include "heartRate.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ============================================================
// ESP-WROOM-32
// ============================================================
#define SDA_PIN 21
#define SCL_PIN 22

MAX30105 sensor;
Adafruit_SH1106G display(128, 64, &Wire, -1);

// ============================================================
// SETTINGS
// ============================================================

// Your previous 50,000 threshold is unnecessarily high.
// With reduced LED power, normal finger IR may be lower.
const long FINGER_THRESHOLD = 10000;

// Valid human HR range
const float MIN_BPM = 40.0;
const float MAX_BPM = 180.0;

// Average last 4 valid beats
const byte RATE_SIZE = 4;

byte rates[RATE_SIZE] = {0};
byte rateIndex = 0;
byte validRates = 0;

unsigned long lastBeat = 0;

float instantBPM = 0;
float averageBPM = 0;

int shownBPM = 0;

bool fingerPresent = false;

unsigned long lastOLED = 0;
unsigned long lastSerial = 0;
unsigned long fingerStart = 0;

// ============================================================
// RESET
// ============================================================

void resetHR()
{
  lastBeat = 0;

  instantBPM = 0;
  averageBPM = 0;
  shownBPM = 0;

  rateIndex = 0;
  validRates = 0;

  for (byte i = 0; i < RATE_SIZE; i++)
    rates[i] = 0;
}

// ============================================================
// OLED
// ============================================================

void drawOLED()
{
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(34, 2);
  display.print("HEART RATE");

  display.drawLine(0, 13, 127, 13, SH110X_WHITE);

  // ==========================================================
  // No finger
  // ==========================================================

  if (!fingerPresent)
  {
    display.setTextSize(1);

    display.setCursor(27, 27);
    display.print("Place finger");

    display.setCursor(29, 42);
    display.print("on sensor");

    display.display();
    return;
  }

  // ==========================================================
  // Finger present but BPM not available yet
  // ==========================================================

  if (shownBPM == 0)
  {
    display.setTextSize(3);
    display.setCursor(43, 20);
    display.print("--");

    display.setTextSize(1);
    display.setCursor(92, 36);
    display.print("BPM");

    display.setCursor(21, 53);
    display.print("Finding pulse...");

    display.display();
    return;
  }

  // ==========================================================
  // BPM available
  // ==========================================================

  display.setTextSize(3);

  if (shownBPM < 100)
    display.setCursor(28, 20);
  else
    display.setCursor(8, 20);

  display.print(shownBPM);

  display.setTextSize(1);
  display.setCursor(92, 36);
  display.print("BPM");

  display.setCursor(30, 53);
  display.print("Measuring");

  display.display();
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
  Serial.println(" ESP32 HEART RATE MONITOR V2");
  Serial.println("================================");

  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // ==========================================================
  // OLED
  // ==========================================================

  if (!display.begin(0x3C, true))
  {
    Serial.println("OLED ERROR");

    while (1)
      delay(1000);
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(35, 25);
  display.print("Starting...");
  display.display();

  // ==========================================================
  // MAX30102
  // ==========================================================

  if (!sensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("MAX30102 ERROR");

    display.clearDisplay();
    display.setCursor(15, 25);
    display.print("MAX30102 ERROR");
    display.display();

    while (1)
      delay(1000);
  }

  // ==========================================================
  // IMPORTANT SENSOR SETTINGS
  // ==========================================================
  //
  // Previous:
  //
  // brightness = 60
  // ADC range  = 4096
  //
  // Your IR was reaching ~230,000.
  //
  // New:
  //
  // brightness = 15
  // ADC range  = 16384
  //
  // This should give checkForBeat() a much cleaner waveform.
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

  // No additional IR override.
  // setup() controls LED current.

  sensor.setPulseAmplitudeRed(0x05);

  sensor.setPulseAmplitudeGreen(0);

  resetHR();

  Serial.println("MAX30102 OK");
  Serial.println("OLED OK");
  Serial.println();
  Serial.println("Place fingertip gently on sensor.");
  Serial.println();

  drawOLED();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Pull samples into FIFO
  sensor.check();

  // ==========================================================
  // Process every sensor sample
  // ==========================================================

  while (sensor.available())
  {
    long ir = sensor.getFIFOIR();

    sensor.nextSample();

    // ========================================================
    // Finger detection
    // ========================================================

    if (ir < FINGER_THRESHOLD)
    {
      if (fingerPresent)
      {
        Serial.println();
        Serial.println("Finger removed.");

        resetHR();
      }

      fingerPresent = false;

      continue;
    }

    // ========================================================
    // New finger
    // ========================================================

    if (!fingerPresent)
    {
      fingerPresent = true;

      fingerStart = millis();

      resetHR();

      Serial.println();
      Serial.println("Finger detected.");
      Serial.println("Stabilizing...");
    }

    // ========================================================
    // Ignore first 1.5 sec
    //
    // Finger placement itself creates a huge optical transient,
    // which caused your previous false "first heartbeat".
    // ========================================================

    if (millis() - fingerStart < 1500)
    {
      continue;
    }

    // ========================================================
    // Beat detection
    // ========================================================

    if (checkForBeat(ir))
    {
      unsigned long now = millis();

      // First REAL beat
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

      instantBPM = 60000.0 / (float)delta;

      // ======================================================
      // Only accept realistic BPM
      // ======================================================

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
        // Average valid beats
        // ====================================================

        int total = 0;

        for (byte i = 0; i < validRates; i++)
        {
          total += rates[i];
        }

        averageBPM = (float)total / validRates;

        shownBPM = round(averageBPM);

        Serial.print("BEAT  | Instant=");
        Serial.print(instantBPM, 1);

        Serial.print(" BPM | Average=");
        Serial.print(shownBPM);

        Serial.println(" BPM");
      }
      else
      {
        Serial.print("Rejected beat: ");
        Serial.print(instantBPM, 1);
        Serial.println(" BPM");
      }
    }

    // ========================================================
    // Diagnostics
    // ========================================================

    if (millis() - lastSerial >= 500)
    {
      lastSerial = millis();

      Serial.print("IR=");
      Serial.print(ir);

      if (shownBPM > 0)
      {
        Serial.print("   HR=");
        Serial.print(shownBPM);
        Serial.println(" BPM");
      }
      else
      {
        Serial.println("   Looking for pulse...");
      }
    }
  }

  // ==========================================================
  // OLED
  //
  // Refresh 4 times/sec only.
  // Sensor continues at 100 samples/sec.
  // ==========================================================

  if (millis() - lastOLED >= 250)
  {
    lastOLED = millis();

    drawOLED();
  }

  delay(1);
}