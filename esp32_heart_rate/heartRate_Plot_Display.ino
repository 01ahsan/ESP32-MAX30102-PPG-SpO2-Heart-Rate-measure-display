#include <Arduino.h>
#include <Wire.h>

#include "MAX30105.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Place your index finger softly on the sensor. Wait for 20-30 seconds for the plot/graph to appear.
// if the output not properly plotted, try restarting the process

#define SDA_PIN 21
#define SCL_PIN 22

// ============================================================
// DEVICES
// ============================================================

MAX30105 sensor;

Adafruit_SH1106G display(
  128,
  64,
  &Wire,
  -1
);

// ============================================================
// SENSOR SETTINGS
// ============================================================

const int SAMPLE_RATE = 100;

const long FINGER_THRESHOLD = 10000;

// ============================================================
//  CALIBRATION
// ============================================================

const unsigned long STABILIZE_MS = 2000;


const int CAL_SAMPLES = 500;

unsigned long fingerStart = 0;


const float BASELINE_ALPHA = 0.010f;

const float SMOOTH_ALPHA_1 = 0.55f;
const float SMOOTH_ALPHA_2 = 0.50f;


float irBaseline = 0.0f;
float redBaseline = 0.0f;

float irSmooth1 = 0.0f;
float redSmooth1 = 0.0f;

float irSmooth2 = 0.0f;
float redSmooth2 = 0.0f;

bool fingerPresent = false;
bool initialized = false;


float irCal[CAL_SAMPLES];
float redCal[CAL_SAMPLES];

int calIndex = 0;

bool calibrationDone = false;

float irCenter = 0.0f;
float redCenter = 0.0f;

float irAmplitude = 100.0f;
float redAmplitude = 100.0f;

// ============================================================
// WAVEFORM SETTINGS
// ============================================================

const float IR_DISPLAY_AMPLITUDE = 0.95f;
const float RED_DISPLAY_AMPLITUDE = 0.65f;

const float IR_SHAPE_GAIN = 2.8f;
const float RED_SHAPE_GAIN = 1.6f;

// ============================================================
// OLED WAVEFORM BUFFER
// ============================================================

#define SCREEN_WIDTH 128

const int IR_CENTER_Y = 20;
const int RED_CENTER_Y = 49;

// Pixel amplitude
const float IR_PIXEL_GAIN = 13.0f;
const float RED_PIXEL_GAIN = 13.0f;

// Store one Y position for every OLED column
int irWaveBuffer[SCREEN_WIDTH];
int redWaveBuffer[SCREEN_WIDTH];

int waveIndex = 0;


const unsigned long OLED_INTERVAL_MS = 50;

unsigned long lastOLEDUpdate = 0;

// ============================================================
// SERIAL PLOTTER
// ============================================================

const int SERIAL_DIVIDER = 2;

int serialCounter = 0;

// ============================================================
// SORT ARRAY
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
// CALIBRATION
// ============================================================

void calculateCalibration()
{
  sortArray(irCal, calIndex);
  sortArray(redCal, calIndex);

  int mid = calIndex / 2;

  irCenter = irCal[mid];
  redCenter = redCal[mid];

  int p10 =
    (int)(0.10f * (calIndex - 1));

  int p90 =
    (int)(0.90f * (calIndex - 1));

  float irLow = irCal[p10];
  float irHigh = irCal[p90];

  float redLow = redCal[p10];
  float redHigh = redCal[p90];

  irAmplitude =
    (irHigh - irLow) * 0.5f;

  redAmplitude =
    (redHigh - redLow) * 0.5f;

  if (irAmplitude < 20.0f)
    irAmplitude = 20.0f;

  if (redAmplitude < 20.0f)
    redAmplitude = 20.0f;

  calibrationDone = true;
}

void clearWaveBuffers()
{
  for (int i = 0; i < SCREEN_WIDTH; i++)
  {
    irWaveBuffer[i] = IR_CENTER_Y;
    redWaveBuffer[i] = RED_CENTER_Y;
  }

  waveIndex = 0;
}


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

  serialCounter = 0;

  clearWaveBuffers();
}


void showMessage(
  const char *line1,
  const char *line2 = nullptr
)
{
  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 18);
  display.println(line1);

  if (line2 != nullptr)
  {
    display.setCursor(0, 34);
    display.println(line2);
  }

  display.display();
}


void addWavePoint(
  float irWave,
  float redWave
)
{

  int irY =
    IR_CENTER_Y -
    (int)(irWave * IR_PIXEL_GAIN);

  int redY =
    RED_CENTER_Y -
    (int)(redWave * RED_PIXEL_GAIN);

  irY = constrain(irY, 8, 31);

  redY = constrain(redY, 38, 61);

  for (int i = 0; i < SCREEN_WIDTH - 1; i++)
  {
    irWaveBuffer[i] =
      irWaveBuffer[i + 1];

    redWaveBuffer[i] =
      redWaveBuffer[i + 1];
  }

  // Add newest sample at right side
  irWaveBuffer[SCREEN_WIDTH - 1] = irY;
  redWaveBuffer[SCREEN_WIDTH - 1] = redY;
}


void drawWaveforms()
{
  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("IR");

  display.setCursor(112, 0);
  display.print("PPG");

  display.drawLine(
    0,
    33,
    127,
    33,
    SH110X_WHITE
  );

  display.setCursor(0, 35);
  display.print("RED");

  // ==========================================================
  // DRAW IR WAVEFORM
  // ==========================================================

  for (int x = 1; x < SCREEN_WIDTH; x++)
  {
    display.drawLine(
      x - 1,
      irWaveBuffer[x - 1],
      x,
      irWaveBuffer[x],
      SH110X_WHITE
    );
  }

  // ==========================================================
  // DRAW RED WAVEFORM
  // ==========================================================

  for (int x = 1; x < SCREEN_WIDTH; x++)
  {
    display.drawLine(
      x - 1,
      redWaveBuffer[x - 1],
      x,
      redWaveBuffer[x],
      SH110X_WHITE
    );
  }

  display.display();
}

void setup()
{
  Serial.begin(115200);

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  Wire.setClock(400000);


  if (!display.begin(0x3C, true))
  {
    while (1)
    {
      delay(1000);
    }
  }

  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);

  showMessage(
    "Starting...",
    "MAX30102 + OLED"
  );


  if (!sensor.begin(
        Wire,
        I2C_SPEED_FAST
      ))
  {
    showMessage(
      "MAX30102 ERROR",
      "Check wiring"
    );

    while (1)
    {
      delay(1000);
    }
  }

  // ==========================================================
  // MAX30102 CONFIGURATION
  // ==========================================================

  byte ledBrightness = 16;

  byte sampleAverage = 4;

  byte ledMode = 2;

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

  showMessage(
    "Place finger",
    "on sensor..."
  );

  delay(500);
}

void loop()
{
  sensor.check();

  while (sensor.available())
  {

    float irRaw =
      (float)sensor.getFIFOIR();

    float redRaw =
      (float)sensor.getFIFORed();

    sensor.nextSample();


    if (irRaw < FINGER_THRESHOLD)
    {
      if (fingerPresent)
      {
        resetSignal();

        showMessage(
          "Place finger",
          "on sensor..."
        );
      }

      fingerPresent = false;

      continue;
    }


    if (!fingerPresent)
    {
      fingerPresent = true;

      fingerStart = millis();

      resetSignal();

      showMessage(
        "Finger detected",
        "Stabilizing..."
      );
    }


    if (!initialized)
    {
      irBaseline = irRaw;
      redBaseline = redRaw;

      initialized = true;

      continue;
    }


    irBaseline +=
      BASELINE_ALPHA *
      (irRaw - irBaseline);

    redBaseline +=
      BASELINE_ALPHA *
      (redRaw - redBaseline);

    float irAC =
      irRaw - irBaseline;

    float redAC =
      redRaw - redBaseline;

    irSmooth1 +=
      SMOOTH_ALPHA_1 *
      (irAC - irSmooth1);

    redSmooth1 +=
      SMOOTH_ALPHA_1 *
      (redAC - redSmooth1);


    irSmooth2 +=
      SMOOTH_ALPHA_2 *
      (irSmooth1 - irSmooth2);

    redSmooth2 +=
      SMOOTH_ALPHA_2 *
      (redSmooth1 - redSmooth2);


    if (
      millis() - fingerStart
      <
      STABILIZE_MS
    )
    {
      continue;
    }


    if (!calibrationDone)
    {
      if (calIndex == 0)
      {
        showMessage(
          "Calibrating...",
          "Keep finger still"
        );
      }

      if (calIndex < CAL_SAMPLES)
      {
        irCal[calIndex] =
          irSmooth2;

        redCal[calIndex] =
          redSmooth2;

        calIndex++;
      }

      if (calIndex >= CAL_SAMPLES)
      {
        calculateCalibration();

        clearWaveBuffers();

        display.clearDisplay();
        display.display();
      }

      continue;
    }

    float irSignal =
      irSmooth2 -
      irCenter;

    float redSignal =
      redSmooth2 -
      redCenter;

    float irNormalized =
      irSignal /
      irAmplitude;

    float redNormalized =
      redSignal /
      redAmplitude;


    float irWave =
      tanhf(
        irNormalized *
        IR_SHAPE_GAIN
      ) *
      IR_DISPLAY_AMPLITUDE;

    float redWave =
      tanhf(
        redNormalized *
        RED_SHAPE_GAIN
      ) *
      RED_DISPLAY_AMPLITUDE;

    irWave = -irWave;
    redWave = -redWave;


    serialCounter++;

    if (
      serialCounter >=
      SERIAL_DIVIDER
    )
    {
      serialCounter = 0;

      Serial.print(
        "IR_Heart:"
      );

      Serial.print(
        irWave,
        4
      );

      Serial.print(",");

      Serial.print(
        "RED_Heart:"
      );

      Serial.println(
        redWave,
        4
      );
    }


    if (
      millis() -
      lastOLEDUpdate
      >=
      OLED_INTERVAL_MS
    )
    {
      lastOLEDUpdate =
        millis();

      addWavePoint(
        irWave,
        redWave
      );

      drawWaveforms();
    }
  }

  delay(1);
}