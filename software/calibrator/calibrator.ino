/********************************************
* This program was developed with help and code from the following sources:
*
* - https://forum.arduino.cc/t/using-millis-for-timing-a-beginners-guide/483573, for creating the time loop
* - https://docs.arduino.cc/language-reference/en/functions/analog-io/analogRead/, for converting digital readings to analog
* - https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide/all, for setup, calibration, and use of the HX711 amplifiers
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

// constant variables
const byte TARE_MEASUREMENTS = 25;
const int PRECISION = 3;
const unsigned long BAUD_RATE = 115200;

// rescaling factors, to be calibrated
float factor1 = 135000;
float factor2 = 170000;
float factor3 = 111000;

// variables for the user to adjust:
unsigned long maxTime = 60;       // recording length in seconds
float adjust1 = 1000;             // automatic adjustment factor for each data collection step
float errorBound = 0.02;          // acceptable two-sided error for weight measurement
float calibrationWeight = 2.93;   // weight of calibration item, in lbs
float convergenceFactor = 0.98;   // factor to multiply adjustment factor by for every acceptable measurement
float delayTime = 50;             // number of ms to pause in between data collection steps

// break adjustment factor up across the three strain gauges
float adjust2;
float adjust3;

// toggle adjustment factor changes
bool toggle = false;


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
  scale_1.set_scale(factor1);
  scale_2.set_scale(factor2);
  scale_3.set_scale(factor3);

  // give 2 seconds of additional time before tare
  delay(2000);

  // reset all strain gauge values to 0
  // based on average of n measurements
  scale_1.tare(TARE_MEASUREMENTS);
  scale_2.tare(TARE_MEASUREMENTS);
  scale_3.tare(TARE_MEASUREMENTS);

  // tell user that Arduino is running
  Serial.println("Online");
  Serial.println("Input recording time in seconds:");

  // wait for user input
  while(!Serial.available()) {
    delay(5);
  }
  
  // interpret input as recording time
  String time_from_RPi = Serial.readStringUntil('\n');
  maxTime = strtoul(time_from_RPi.c_str(), NULL, 10);
  Serial.println("Confirming recording time: " + String(maxTime));

  Serial.println("Input automatic adjustment factor:");

  // wait for user input
  while(!Serial.available()) {
    delay(5);
  }
  
  // interpret input as adjustment factor
  String adjust_factor = Serial.readStringUntil('\n');
  adjust1 = strtoul(adjust_factor.c_str(), NULL, 10);
  adjust2 = adjust1;
  adjust3 = adjust2;

  Serial.println("Confirming recording time: " + String(adjust1));

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
      printVariables();
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
* Performs one data collection step, and prints the strain gauge readings and rescaling factors.
* Automatically calibrates strain gauges if toggled.
*
********************************************/
void printVariables() {
  // use new rescaling factor
  scale_1.set_scale(factor1);
  scale_2.set_scale(factor2);
  scale_3.set_scale(factor3);

  // take 6 measurements for each strain gauge and average them
  float strain1 = scale_1.get_units(6);
  float strain2 = scale_2.get_units(6);
  float strain3 = scale_3.get_units(6);

  // print weight and adjustment factors to Serial Monitor
  Serial.println("-----------------------------");
  Serial.println(strain1, 3);
  Serial.println(strain2, 3);
  Serial.println(strain3, 3);

  Serial.println(factor1);
  Serial.println(factor2);
  Serial.println(factor3);

  // check for user input
  // if 0 - tare strain gauges
  // if t - toggle automatic rescaling factor adjustments

  // Automatic calibration should only be toggled once the calibration weight 
  // has been put on the mouthpiece.
  if (Serial.available()) {
    char next = Serial.read();
    if (next == 't')
      toggle = !toggle;
    else if (next == '0') {
      digitalWrite(STATUS_LED, LOW);
      digitalWrite(RECORD_LED, LOW);

      scale_1.tare(TARE_MEASUREMENTS);
      scale_2.tare(TARE_MEASUREMENTS);
      scale_3.tare(TARE_MEASUREMENTS);

      Serial.println("Finished");
      digitalWrite(STATUS_LED, HIGH);
    }
  }

  // Every time that the measurement has too high of an error,
  // increase or decrease the rescaling factor accordingly.
  // If the measurement is within the acceptable bound, decrease the
  // adjustment factor to slowly converge on the correct rescaling factor.

  // This should only be toggled once the calibration weight has been put 
  // on the mouthpiece.
  if (toggle == true) {
    float upperBound = (1 + errorBound) * (calibrationWeight / 3);
    float lowerBound = (1 - errorBound) * (calibrationWeight / 3);

    if (strain1 < lowerBound)
      factor1 -= adjust1;
    else if (strain1 > upperBound)
      factor1 += adjust1;
    else adjust1 *= convergenceFactor;

    if (strain2 < lowerBound)
      factor2 -= adjust2;
    else if (strain2 > upperBound)
      factor2 += adjust2;
    else adjust2 *= convergenceFactor;

    if (strain3 < lowerBound)
      factor3 -= adjust3;
    else if (strain3 > upperBound)
      factor3 += adjust3;
    else adjust3 *= convergenceFactor;
  }

  delay(delayTime);
}