/********************************************
* This program was developed with help and code from the following sources:
*
* - https://forum.arduino.cc/t/using-millis-for-timing-a-beginners-guide/483573, for creating the time loop
* - https://docs.arduino.cc/language-reference/en/functions/analog-io/analogRead/, for converting digital readings to analog
* - https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide/all, for setup and use of the HX711 amplifiers
*
* This program makes use of the HX711 library by Rob Tillaart:
* https://github.com/RobTillaart/HX711
*
********************************************/

#include "HX711.h"

// initalize the three strain gauges
HX711 scale_1;
HX711 scale_2;
HX711 scale_3;

// pin number declarations
const int CLK_1 = 10;
const int CLK_2 = 11;
const int CLK_3 = 2;

const int DOUT_1 = 3;
const int DOUT_2 = 4;
const int DOUT_3 = 5;

const int ON_LED = 8;
const int STATUS_LED = 7;
const int RECORD_LED = 6;

const uint8_t DIFF = A0;
const uint8_t GAUGE = A1;

// constant variables
const float V_SUPP = 5;
const float PMAX_DIFF = 6.89476; // 1 psi
const float PMIN_DIFF = -6.89476; // -1 psi
const float PMAX_GAUGE = 6.89476; // 1 psi
const float PMIN_GAUGE = 0;

const float FACTOR_1 = 135000;
const float FACTOR_2 = 170000;
const float FACTOR_3 = 111000;

const byte TARE_MEASUREMENTS = 25;
const int PRECISION = 3;

const unsigned long BAUD_RATE = 115200;

// time variable, to be received from RPi4B
unsigned long maxTime = 60; // seconds


// run once on startup
void setup() {
  // designate LED pins as outputs
  pinMode(ON_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(RECORD_LED, OUTPUT);

  // show that Arduino is on, but not ready to record
  digitalWrite(ON_LED, HIGH);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(RECORD_LED, LOW);

  Serial.begin(BAUD_RATE);

  // scale performance seems better when this exists
  // pause for 2 seconds
  delay(2000);

  // initalize strain gauge pins
  scale_1.begin(DOUT_1, CLK_1);
  scale_2.begin(DOUT_2, CLK_2);
  scale_3.begin(DOUT_3, CLK_3);

  // set gain to 64 - DOES NOT WORK WITH 128
  scale_1.set_gain(64, true);
  scale_2.set_gain(64, true);
  scale_3.set_gain(64, true);

  // set rescaling factor
  scale_1.set_scale(FACTOR_1);
  scale_2.set_scale(FACTOR_2);
  scale_3.set_scale(FACTOR_3);

  // give 2 seconds of additional time before tare
  delay(2000);

  // reset all strain gauge values to 0
  // based on average of n measurements
  scale_1.tare(TARE_MEASUREMENTS);
  scale_2.tare(TARE_MEASUREMENTS);
  scale_3.tare(TARE_MEASUREMENTS);

  // tell RPi to continue onward
  Serial.println("Online");

  // wait for input from RPi
  while(!Serial.available()) {
    delay(5);
  }
  
  // interpret input as recording time
  String time_from_RPi = Serial.readStringUntil('\n');
  maxTime = strtoul(time_from_RPi.c_str(), NULL, 10);
  Serial.println(maxTime);

  // indicate that recording can begin
  digitalWrite(STATUS_LED, HIGH);
}


// run indefinitely
void loop() {
  // check for serial input from RPi
  String nextLine = "";
  if (Serial.available()) {
    nextLine = Serial.readStringUntil('\n');
  }

  // tare the scale if tare button is pressed
  if (nextLine.equals("Zero") || nextLine.equals("Tare")) {
    digitalWrite(STATUS_LED, LOW);
    digitalWrite(RECORD_LED, LOW);

    scale_1.tare(TARE_MEASUREMENTS);
    scale_2.tare(TARE_MEASUREMENTS);
    scale_3.tare(TARE_MEASUREMENTS);

    Serial.println("Finished");
    digitalWrite(STATUS_LED, HIGH);
  } else if (nextLine.equals("Start")) {
    // begin recordings if start button is pressed
    unsigned long startTime = millis();
    digitalWrite(STATUS_LED, LOW);
    digitalWrite(RECORD_LED, HIGH);

    // recording time loop
    unsigned long timeDifference = 0;
    while (timeDifference < (maxTime * 1000)) {
      printVariables(timeDifference);
      timeDifference = millis() - startTime;
    }

    // indicate that recording is done
    Serial.println("Ending");
    digitalWrite(STATUS_LED, HIGH);
    digitalWrite(RECORD_LED, LOW);
  } else {
    digitalWrite(RECORD_LED, LOW);
  }
}


/********************************************
*
* Performs one data collection step, and sends the pressure and strain gauge data
* via serial to the Raspberry Pi.
*
* @param time: number of milliseconds (ms) since the start of the recording session
*
********************************************/
void printVariables(unsigned long time) {
  // take 1 single measurement for each strain gauge
  float strain1 = scale_1.get_units();
  float strain2 = scale_2.get_units();
  float strain3 = scale_3.get_units();

  // read values from pressure sensors
  int readDiff = analogRead(DIFF);
  int readGauge = analogRead(GAUGE);

  // convert digital to analog (0-1023 to 0-5V)
  float vDiff = readDiff * V_SUPP / 1023.0;
  float vGauge = readGauge * V_SUPP / 1023.0;

  // formula is supplied by Honeywell
  float pDiff = ((vDiff - (0.1 * V_SUPP)) / ((0.8 * V_SUPP) / (PMAX_DIFF - PMIN_DIFF))) + PMIN_DIFF;
  float pGauge = ((vGauge - (0.1 * V_SUPP)) / ((0.8 * V_SUPP) / (PMAX_GAUGE - PMIN_GAUGE))) + PMIN_GAUGE;

  // print to RPi
  Serial.println("Data");
  Serial.println(time);
  Serial.println(pDiff, PRECISION);
  Serial.println(pGauge, PRECISION);
  Serial.println(strain1, PRECISION);
  Serial.println(strain2, PRECISION);
  Serial.println(strain3, PRECISION);
}