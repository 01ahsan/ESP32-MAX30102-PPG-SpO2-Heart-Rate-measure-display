#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"

#define SDA_PIN 21
#define SCL_PIN 22

MAX30105 sensor;

// ============================================================
// Open serial plotter from tools and select baud rate to 115200
// Place your index finger softly on the sensor. Wait for 20-30 seconds for the plot/graph to appear.
// if the output not properly plotted, try restarting the process

const int SAMPLE_RATE = 100;
const long FINGER_THRESHOLD = 10000;

// Plot 50 points/sec
const int PLOT_DIVIDER = 2;
int plotCounter = 0;


const unsigned long STABILIZE_MS = 2000;
const int CAL_SAMPLES = 500;   // 5 sec @ 100 Hz

unsigned long fingerStart = 0;

const float BASELINE_ALPHA = 0.010f;

const float SMOOTH_ALPHA_1 = 0.55f;
const float SMOOTH_ALPHA_2 = 0.50f;

// ============================================================
// SIGNAL STATE
// ============================================================

float irBaseline = 0.0f;
float redBaseline = 0.0f;

float irSmooth1 = 0.0f;
float redSmooth1 = 0.0f;

float irSmooth2 = 0.0f;
float redSmooth2 = 0.0f;

bool fingerPresent = false;
bool initialized = false;

// ============================================================
// CALIBRATION
// ============================================================

float irCal[CAL_SAMPLES];
float redCal[CAL_SAMPLES];

int calIndex = 0;
bool calibrationDone = false;

float irCenter = 0.0f;
float redCenter = 0.0f;

float irAmplitude = 100.0f;
float redAmplitude = 100.0f;

// ============================================================
// VISUAL SETTINGS
// ============================================================

const float IR_OFFSET  = 1.25f;
const float RED_OFFSET = -1.25f;

// Independent display amplitudes
const float IR_DISPLAY_AMPLITUDE  = 0.95f;
const float RED_DISPLAY_AMPLITUDE = 0.65f;

// Independent soft gains
const float IR_SHAPE_GAIN  = 2.8f;
const float RED_SHAPE_GAIN = 1.6f;

// ============================================================
// SORT
// ============================================================

void sortArray(float *data, int n)
{
  for (int i = 1; i < n; i++)
  {
    float value = data[i];
    int j = i - 1;

    while (j >= 0 && data[j] > value)
    {
      data[j + 1] = data[j];
      j--;
    }

    data[j + 1] = value;
  }
}

// ============================================================
// ROBUST CALIBRATION
// ============================================================

void calculateCalibration()
{
  sortArray(irCal, calIndex);
  sortArray(redCal, calIndex);

  int mid = calIndex / 2;

  irCenter = irCal[mid];
  redCenter = redCal[mid];

  int p10 = (int)(0.10f * (calIndex - 1));
  int p90 = (int)(0.90f * (calIndex - 1));

  float irLow = irCal[p10];
  float irHigh = irCal[p90];

  float redLow = redCal[p10];
  float redHigh = redCal[p90];

  irAmplitude = (irHigh - irLow) * 0.5f;
  redAmplitude = (redHigh - redLow) * 0.5f;

  if (irAmplitude < 20.0f)
    irAmplitude = 20.0f;

  if (redAmplitude < 20.0f)
    redAmplitude = 20.0f;

  calibrationDone = true;
}

// ============================================================
// RESET
// ============================================================

void resetSignal()
{
  irBaseline = 0.0f;
  redBaseline = 0.0f;

  irSmooth1 = 0.0f;
  redSmooth1 = 0.0f;

  irSmooth2 = 0.0f;
  redSmooth2 = 0.0f;

  irCenter = 0.0f;
  redCenter = 0.0f;

  irAmplitude = 100.0f;
  redAmplitude = 100.0f;

  initialized = false;

  calibrationDone = false;
  calIndex = 0;

  plotCounter = 0;
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!sensor.begin(Wire, I2C_SPEED_FAST))
  {
    while (1)
    {
      delay(1000);
    }
  }

  // ==========================================================
  // MAX30102 SETTINGS
  // ==========================================================

  byte ledBrightness = 16;
  byte sampleAverage = 4;
  byte ledMode = 2;      // RED + IR

  int sampleRate = SAMPLE_RATE;
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

  sensor.setPulseAmplitudeIR(0x10);
  sensor.setPulseAmplitudeRed(0x10);
  sensor.setPulseAmplitudeGreen(0);

  resetSignal();

  delay(1000);
}

void loop()
{
  sensor.check();

  while (sensor.available())
  {
    float irRaw = (float)sensor.getFIFOIR();
    float redRaw = (float)sensor.getFIFORed();

    sensor.nextSample();

    if (irRaw < FINGER_THRESHOLD)
    {
      if (fingerPresent)
      {
        resetSignal();
      }

      fingerPresent = false;
      continue;
    }

    if (!fingerPresent)
    {
      fingerPresent = true;
      fingerStart = millis();

      resetSignal();
    }


    if (!initialized)
    {
      irBaseline = irRaw;
      redBaseline = redRaw;

      initialized = true;
      continue;
    }


    irBaseline +=
      BASELINE_ALPHA * (irRaw - irBaseline);

    redBaseline +=
      BASELINE_ALPHA * (redRaw - redBaseline);

    float irAC = irRaw - irBaseline;
    float redAC = redRaw - redBaseline;

    // ========================================================
    // 2. LIGHT SMOOTHING
    // ========================================================

    irSmooth1 +=
      SMOOTH_ALPHA_1 * (irAC - irSmooth1);

    redSmooth1 +=
      SMOOTH_ALPHA_1 * (redAC - redSmooth1);

    irSmooth2 +=
      SMOOTH_ALPHA_2 * (irSmooth1 - irSmooth2);

    redSmooth2 +=
      SMOOTH_ALPHA_2 * (redSmooth1 - redSmooth2);

    // ========================================================
    // 3. STABILIZATION
    // ========================================================

    if (millis() - fingerStart < STABILIZE_MS)
    {
      continue;
    }

    // ========================================================
    // 4. CALIBRATION
    // ========================================================

    if (!calibrationDone)
    {
      if (calIndex < CAL_SAMPLES)
      {
        irCal[calIndex] = irSmooth2;
        redCal[calIndex] = redSmooth2;

        calIndex++;
      }

      if (calIndex >= CAL_SAMPLES)
      {
        calculateCalibration();
      }

      continue;
    }

    // ========================================================
    // 5. CENTER SIGNAL
    // ========================================================

    float irSignal = irSmooth2 - irCenter;
    float redSignal = redSmooth2 - redCenter;

    // ========================================================
    // 6. NORMALIZE
    // ========================================================

    float irNormalized =
      irSignal / irAmplitude;

    float redNormalized =
      redSignal / redAmplitude;

    // ========================================================
    // 7. SOFT GAIN
    // ========================================================

    float irWave =
      tanhf(irNormalized * IR_SHAPE_GAIN) *
      IR_DISPLAY_AMPLITUDE;

    float redWave =
      tanhf(redNormalized * RED_SHAPE_GAIN) *
      RED_DISPLAY_AMPLITUDE;

    // ========================================================
    // 8. FLIP UPWARD
    // ========================================================

    irWave = -irWave;
    redWave = -redWave;

    // ========================================================
    // 9. VERTICAL OFFSETS
    // ========================================================

    float irPlot =
      IR_OFFSET + irWave;

    float redPlot =
      RED_OFFSET + redWave;

    // ========================================================
    // 10. SERIAL PLOTTER
    // ========================================================

    plotCounter++;

    if (plotCounter >= PLOT_DIVIDER)
    {
      plotCounter = 0;

      Serial.print("IR_Heart:");
      Serial.print(irPlot, 4);

      Serial.print(",");

      Serial.print("RED_Heart:");
      Serial.println(redPlot, 4);
    }
  }

  delay(1);
}