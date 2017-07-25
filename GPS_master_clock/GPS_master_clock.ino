/*
  GPS Master clock
  (C) Andy Doswell 22nd July 2016
  License: The MIT License (See full license at the bottom of this file)

  Schematic can be found at www.andydoz.blogspot.com/


  Uses the very excellent TinyGPS++ library from http://arduiniana.org/libraries/tinygpsplus/
  and also the fabulous Virtualwire library from https://www.pjrc.com/teensy/td_libs_VirtualWire.html
  and the most superb I2C LCD library from https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads

  pin 0 (RX)  is connected to the GPS TX pin.
  A3 is connected to a "push to transmit" button.
  Pin 10 is connected to the transmitter (433MHz or whatever's legal in your area)
  LCD is a 20 x 4 I2C display fitted with PCF8574 IC

  Sketch transmits a clock signal at a random time (never more than 24 hours) to sync up other clocks.

*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <VirtualWire.h>
#include <VirtualWire_Config.h>

LiquidCrystal_I2C  lcd(0x3f,16,2); // I2C addreSerial 0x3f, 20x4 display
TinyGPSPlus gps; //TinyGPS++ claSerial
static const uint32_t GPSBaud = 9600; //Our GPS baudrate.
int hourTen; //tens of hours
int hourUnit; //units of hours
int minTen; // you get the idea..
int minUnit;
int secTen;
int secUnit;
unsigned long transmitTimer;
unsigned long secsPastMidnight;
unsigned long SerialTimeout = 0;
unsigned long secsToMidnight;
unsigned long timerPrint;
boolean displayLoop = false;

struct TXData
{
  int TX_ID;
  int Hours;
  int Mins;
  int Secs;
  int Day;
  int Month;
  int Year;
};

void setup() {

  Serial.begin(GPSBaud); // start the comms with the GPS Rx
  transmitTimer = random(0, 86399); // transmit sometime in the next 24 hours
  lcd.begin(20, 4); // set up the LCD as 20 x 4 display
//  lcd.setBacklightPin(3, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
  // Virtualwire setup
  vw_set_tx_pin(10); // TX pin set to 10
  vw_set_rx_pin(9); // RX pin set to a spare pin, even though we don't use it.
  vw_setup(1200); //sets virtualwire for a tx rate of 1200 bits per second
  pinMode (A3, INPUT); // push to transmit connected to A3 and GND
  digitalWrite (A3, HIGH); // A3 pulled high
  randomSeed(analogRead(A0));
  transmitTimer = random(0, 86399); // set a random transmit time.
}

void displayData() {

  SerialTimeout = millis();
  hourUnit = (gps.time.hour() % 10);
  hourTen = ((gps.time.hour() / 10) % 10);
  minUnit = (gps.time.minute() % 10);
  minTen = ((gps.time.minute() / 10) % 10);
  secUnit = (gps.time.second() % 10);
  secTen = ((gps.time.second() / 10) % 10);
  secUnit = (gps.time.second() % 10);

  if (displayLoop == false) { // this bit of code is to overcome a bit of an odd bug, and I don't know why it happens,
    lcd.setCursor (0, 1);   // but Satellites in view and HDOP return 0 if you get them along with location, regardless of order. So we only update the position every 100 cycles.
    lcd.print("  ");
    lcd.print (gps.location.lat(), 4);
    lcd.print (" ");
    lcd.print (gps.location.lng(), 4);
    lcd.print ("   ");
    displayLoop = true;
  }
  else {
    lcd.setCursor (0, 2);
    lcd.print ("  Sat:");
    lcd.print (gps.satellites.value());
    lcd.print (" HDOP:");
    lcd.print (gps.hdop.value());
    lcd.print (" ");
    displayLoop = false;
  }
  lcd.setCursor(0, 0);
  lcd.print (" ");
  lcd.print(gps.date.day());
  lcd.print("/");
  lcd.print(gps.date.month());
  lcd.print("/");
  lcd.print(gps.date.year());
  lcd.print(" ");
  lcd.print(hourTen);
  lcd.print(hourUnit);
  lcd.print(":");
  lcd.print(minTen);
  lcd.print (minUnit);
  lcd.print (":");
  lcd.print (secTen);
  lcd.print (secUnit);
  lcd.setCursor (0, 3);
  lcd.print ("    TX in:");

  lcd.print (timerPrint);
  lcd.print (" ");

}

void displayNoSerial() {
  lcd.clear();
  lcd.print ("ERROR");
  lcd.setCursor (0, 1);
  lcd.print ("Serial data comms");
  lcd.setCursor (0, 2);
  lcd.print ("Check cable");
  delay (2000);
}

void displayNoGPS() {
  lcd.clear();
  lcd.print("Warning");
  lcd.setCursor(0, 1);
  lcd.print("No or poor GPS data");
  lcd.setCursor(0, 2);
  lcd.print("Sats in view=");
  lcd.print (gps.satellites.value());
}

void transmitTime () { // transmits the current time and date.

  struct TXData payload; //Loads the Data struct with the payload data
  lcd.clear ();
  lcd.print ("Transmitting clock sync");
  lcd.setCursor (0, 1);
  payload.TX_ID = 1;
  payload.Hours = gps.time.hour();
  payload.Mins = gps.time.minute();
  payload.Secs = gps.time.second();
  payload.Day = gps.date.day();
  payload.Month = gps.date.month();
  payload.Year = gps.date.year();
  vw_send((uint8_t *)&payload, sizeof(payload)); //Transmits the struct
  vw_wait_tx(); //waits for the TX to complete

  transmitTimer = random(0, 86399);
  lcd.clear();
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial.available())
      gps.encode(Serial.read());
  } while (millis() - start < ms);
}

void loop()
{

  smartDelay (0); // get the gps data

  if (millis() > SerialTimeout + 1500 ) { // detects if the Serial comms has failed. // if the position hasn't become valid in 1.5 seconds then display No serial.

    displayNoSerial();

    SerialTimeout = millis (); // reset the serial time out
  }
  secsPastMidnight =  gps.time.second() + (gps.time.minute() * 60) + (gps.time.hour() * 3600UL); // number of seconds past midnight
  secsToMidnight = 86399UL - secsPastMidnight ; // number of second to midnight.

  if (transmitTimer >= secsPastMidnight) { // ensures the seconds to TX display is correct.
    timerPrint = transmitTimer - secsPastMidnight;
  }
  else {
    timerPrint = transmitTimer + secsToMidnight;
  }

  if (secsPastMidnight == transmitTimer) { //if it's time to transmit, do it.
    transmitTime();
  }

  if (digitalRead (A3) == LOW) { // Checks the "push to transmit" button, and transmits if it's low.
    transmitTime();
  }

  if (gps.location.isUpdated() ) { // if there's new data, display it.
    displayData();
  }

}

/*
   Copyright (c) 2016 Andrew Doswell

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permiSerialion notice shall be included in
   all copies or substantial portions of the Software.

   Any commercial use is prohibited without prior arrangement.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESerial FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/
