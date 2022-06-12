#include <LiquidCrystal.h>
#include "Adafruit_VL53L0X.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_24bargraph bar = Adafruit_24bargraph();
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 6, 5, 4, 3);

int solenoidPin = 12;
int buttonPin = 10;
int buttonState = 0;
int autoFill = false;
int btnOn = false;
int filledInRow = 0;
int counter = 0;
int dayCounter = 0;
float filledToday = 0.0;
float filledYear = 0.0;
static char outDay[4];
static char outYear[5];
uint16_t waterDistanceMm;


// Configuration values:
#define TANK_HEIGHT_MM      285
#define SECOND              1000
#define MINUTE              60000
#define MINS_BETWEEN_FILLS  30
#define SECS_TO_FILL        10


void setup(void) {
  if (!lox.begin()) {
    while (1);
  }

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("Gallons ");
  lcd.setCursor(8, 0);
  dtostrf(filledToday, 2, 1, outDay);
  lcd.print("Day " + String(outDay));
  lcd.setCursor(0, 1);
  dtostrf(filledYear, 3, 1, outYear);
  lcd.print("Year " + String(outYear));
  Serial.begin(9600);
  Serial.println("Begin");

  waterDistanceMm = TANK_HEIGHT_MM;
  counter = 0;

  //Sets the pin as an output
  pinMode(solenoidPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  bar.begin(0x70);  // pass in the address

  for (uint8_t b = 0; b < 24; b++ ) {
    if ((b % 3) == 0)  bar.setBar(b, LED_RED);
    if ((b % 3) == 1)  bar.setBar(b, LED_YELLOW);
    if ((b % 3) == 2)  bar.setBar(b, LED_GREEN);
  }
  bar.writeDisplay();

  delay(2000);
}


void loop(void) {

  // fill if button pressed
  checkButton();

  if (btnOn == true) {
    Serial.println("btnOn true, return " );
    return;
  }
  Serial.print("btnOn false, continue ");
  Serial.println(counter);

  // get distance from range sensor
  getRange();

  // get percent full of water level
  printFullness();

  counter++;

  // if MINS_BETWEEN_FILLS mins of checking have passed, fill and reset counter
  if ((counter / 60) >= MINS_BETWEEN_FILLS) {
    fillIfNeeded();
    counter = 0;
    dayCounter++;
    // every 30 mins for 24 hours
    if (dayCounter >= 48) {
      filledToday = 0;
      dayCounter = 0;
    }
  } else {
    printWait(counter / 60);
  }

  delay(SECOND);

}

void checkButton() {
  buttonState = digitalRead(buttonPin);
  if (!btnOn && buttonState == HIGH) {
    Serial.println("btn flipped, turn pump on" );
    btnOn = true;
    pumpOn();
  } else if (btnOn && buttonState != HIGH) {
    Serial.println("btn flipped, turn pump off" );
    btnOn = false;
    pumpOff();
    filledToday += 0.15625;
    filledYear += 0.15625;
    updateFillTotals();
    counter = 0;
  }
  //  if (autoFill == true) {
  //    autoFill = false;
  //    fill(10);
  //  }
}

void printWait(int count) {
  int minsLeft = MINS_BETWEEN_FILLS - count;
  //  lcd.setCursor(9, 0);
  //  lcd.print("wait");
  //  Serial.println("wait");
  lcd.setCursor(13, 1);
  lcd.print(char(0x00 + 171));
  if (minsLeft < 10) {
    lcd.print("0" + String(minsLeft));
  } else {
    lcd.print(String(minsLeft));
  }

}

void printFill(int count) {
  int secs = count / SECOND;
  //  lcd.setCursor(9, 0);
  //  lcd.print("wait");
  //  Serial.println("wait");
  lcd.setCursor(13, 1);
  lcd.print(char(0x00 + 171));
  if (secs < 10) {
    lcd.print("0" + String(secs));
  } else {
    lcd.print(String(secs));
  }
}

void printFullness() {
  int p = (1 - ((float)waterDistanceMm / (float)TANK_HEIGHT_MM)) * 100;
  //  String percentFull = String(p) + "% FULL  ";
  //  lcd.setCursor(0, 0);
  //  lcd.print(percentFull);
  setBar(p);
}

void setBar(int percent) {
  int ledLevel = map(percent, 0, 90, 0, 24);
  // loop over the LED array:
  for (int thisLed = 0; thisLed < 24; thisLed++) {
    // if the array element's index is less than ledLevel,
    // turn the pin for this element on:
    if (thisLed < ledLevel) {
      if (thisLed < 4)  bar.setBar(23 - thisLed, LED_RED);
      else if (thisLed < 12)  bar.setBar(23 - thisLed, LED_YELLOW);
      else bar.setBar(23 - thisLed, LED_GREEN);
    }
    // turn off all pins higher than the ledLevel:
    else {
      bar.setBar(23 - thisLed, LED_OFF);
    }
  }
  bar.writeDisplay();
}

void countdownSec(int secs) {
  while (secs >= 1) {
    secs = secs - 1;
    lcd.setCursor(14, 0);
    if (secs < 10) {
      lcd.print("0" + String(secs));
    } else {
      lcd.print(String(secs));
    }
    delay(SECOND);
  }
}

void getRange() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    waterDistanceMm = measure.RangeMilliMeter;
    // lcd.setCursor(0, 1);
    String distString = "Dist (mm): " + String(waterDistanceMm) + "   ";
    Serial.println(distString);
    // lcd.print(distString);
  } else {
    // lcd.setCursor(0, 1);
    // lcd.print(" out of range   ");
    Serial.println(" out of range   ");

  }
}

void fill(int fillTime) {
  pumpOn();
  countdownSec(fillTime);
  pumpOff();
}

void updateFillTotals() {
  lcd.setCursor(8, 0);
  dtostrf(filledToday, 2, 1, outDay);
  lcd.print("Day " + String(outDay));
  lcd.setCursor(0, 1);
  dtostrf(filledYear, 3, 1, outYear);
  lcd.print("Year " + String(outYear));
}

void pumpOn() {
  digitalWrite(solenoidPin, HIGH);    //Switch Solenoid ON
}

void pumpOff() {
  digitalWrite(solenoidPin, LOW);    //Switch Solenoid ON
}

void fillIfNeeded() {
  int currentFill = (1 - ((float)waterDistanceMm / (float)TANK_HEIGHT_MM)) * 100;

  if (currentFill < 50) {
    // fill for SECS_TO_FILL
    if (filledInRow < 3) {
      // lcd.setCursor(9, 0);
      // lcd.print("fill");
      pumpOn();
      countdownSec(SECS_TO_FILL);
      filledInRow++;
    } else {
      filledInRow = 0;
    }
  }
  pumpOff();
  // update fill amounts
  // 20oz per fill (0.15625 of a gallon)
  filledToday += 0.15625;
  filledYear += 0.15625;
  updateFillTotals();
}
