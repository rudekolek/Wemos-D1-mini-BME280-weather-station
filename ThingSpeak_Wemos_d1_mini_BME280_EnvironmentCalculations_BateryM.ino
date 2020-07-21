/*
BME280 I2C Connections

This code shows how to record data from the BME280 environmental sensor
using I2C interface. This file is an example file, part of the Arduino
BME280 library.

GNU General Public License

Written: Dec 30 2015.
Last Updated: Oct 07 2017.

Connecting the BME280 Sensor:
Sensor              ->  Board
-----------------------------
Vin (Voltage In)    ->  3.3V
Gnd (Ground)        ->  Gnd
SDA (Serial Data)   ->  A4 on Uno/Pro-Mini, 20 on Mega2560/Due, 2 Leonardo/Pro-Micro
SCK (Serial Clock)  ->  A5 on Uno/Pro-Mini, 21 on Mega2560/Due, 3 Leonardo/Pro-Micro

 */

/*
  WriteMultipleFields
  
  Description: Writes values to fields 1,2,3,4 and status in a single ThingSpeak update every 20 seconds.
  
  Hardware: ESP8266 based boards
  
  !!! IMPORTANT - Modify the secrets.h file for this project with your network connection and ThingSpeak channel details. !!!
  
  Note:
  - Requires ESP8266WiFi library and ESP8622 board add-on. See https://github.com/esp8266/Arduino for details.
  - Select the target hardware from the Tools->Board menu
  - This example is written for a network using WPA encryption. For WEP or WPA, change the WiFi.begin() call accordingly.
  
  ThingSpeak ( https://www.thingspeak.com ) is an analytic IoT platform service that allows you to aggregate, visualize, and 
  analyze live data streams in the cloud. Visit https://www.thingspeak.com to sign up for a free account and create a channel.  
  
  Documentation for the ThingSpeak Communication Library for Arduino is in the README.md folder where the library was installed.
  See https://www.mathworks.com/help/thingspeak/index.html for the full ThingSpeak documentation.
  
  For licensing information, see the accompanying license file.
  
  Copyright 2018, The MathWorks, Inc.
*/
/*
 * Wemos battery shield, measure Vbat
 * add 100k between Vbat and ADC
 * Voltage divider of 100k+220k over 100k
 * gives 100/420k
 * ergo 4.2V -> 1Volt
 * Max input on A0=1Volt ->1023
 * 4.2*(Raw/1023)=Vbat
 */
 
#include <Wire.h>
#include <BME280I2C.h>

#include <ESP8266WiFi.h>

#include "ThingSpeak.h"
#include "secrets.h" // file with WIFI and ThingSpeak credentials

#include <EnvironmentCalculations.h>

// ThingSpeak Initialize our values
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

// WIFI definition
char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiClient  client;

//BME280 settings
BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

BME280::TempUnit tempUnit(BME280::TempUnit_Celsius); // set the fields with the values
BME280::PresUnit presUnit(BME280::PresUnit_Pa); // set the fields with the values

float temp(NAN), hum(NAN), pres(NAN); // Definition of BME280 data

// EnvironmentCalculations
EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
EnvironmentCalculations::TempUnit envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
float seaLevel = 0.00;
float dewPoint = 0.00;
float absoluteHumidity = 0.00;
float altitude = 104; //Meteo station elevation in meters
                  
// Serial  definition
#define SERIAL_BAUD 115200 //Set serial speed


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(A0, INPUT); //Analog PIN input for batery monitor via 220 Ohm
  Wire.begin(4,5); // Connect I2C devices to SCL(GPIO5), SDA(GPIO4)

  WiFi.mode(WIFI_STA);
   
  Serial.begin(SERIAL_BAUD);  // Initialize serial
  //Serial.setDebugOutput(true); //Debug mode active
  delay(100);

  while(!Serial) {} // Wait for serial begin

  Serial.println("\nESP8266 in normal mode\n");

  while(!bme.begin()) {
  // Print BME280 sensor status to serial
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
  // bme.chipID(); // Deprecated. See chipModel().
  switch(bme.chipModel()) {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       delay(1000);
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       delay(1000);
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }
  
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}


// BME280
void printBME280Data(Stream* client) { // BME 280 data dealing
   bme.read(pres, temp, hum, tempUnit, presUnit);
   
   //seaLevel = EnvironmentCalculations::SealevelAlitude(altitude, temp, pres); // Deprecated. See EquivalentSeaLevelPressure().
   seaLevel = EnvironmentCalculations::EquivalentSeaLevelPressure(altitude, temp, pres) / 100;
   dewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
   absoluteHumidity = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
   
   client->print("Temp: ");
   client->print(temp);
   client->print("°"+ String(tempUnit == BME280::TempUnit_Celsius ? 'C' :'F'));
   client->print("\t\tHumidity: ");
   client->print(hum);
   client->print("%RH");
   client->print("\t\tAbsolute Humidity: ");
   client->print(absoluteHumidity);
   client->print("g/m³\n");
   
   client->print("Pressure: ");
   client->print(pres / 100);
   client->print(String( presUnit == BME280::PresUnit_hPa ? "hPa" :"Pa")); // expected hPa and Pa only
   client->print("\t\tEquivalent Sea Level Pressure: ");
   client->print(seaLevel);
   client->print(String( presUnit == BME280::PresUnit_hPa ? "hPa" :"Pa") + "\n"); // expected hPa and Pa only
   
   client->print("Dew point: ");
   client->print(dewPoint);
   client->print("°"+ String(tempUnit == BME280::TempUnit_Celsius ? 'C' :'F') +"\n");
}


// Battery Monitor
unsigned int a0read=0;
float volt=0.000;

void printBatMonData(Stream* client) { //Battery voltage calculation and print
  a0read = analogRead(A0);
  delay(100);
  volt = (a0read/1023.0) * 5.265; // 5.265 Empiric number of Voltage
  client->print("Battery Voltage: ");
  client->print(volt);
  client->print("V\t\t");
  client->print(a0read);
  client->print("\n");
}


//WIFI
void connectWifiIfNotConnected(unsigned long timeoutMs = 60000) { //Connect to WiFi, if not possible go to sleep for a while to not drain battery.
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.begin(ssid, pass);
  uint8_t startAt = millis();
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(SECRET_SSID);
  Serial.printf("Connecting to WiFi...\t\tConnection status: %d\n", WiFi.status());
  //Serial.println("Connecting to WiFi..." + String(WiFi.status()));
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
//If WIFI faild break go to sleep try again after wakeup.
    if (millis() - startAt > timeoutMs) {
      Serial.println("\nCannot connect to WiFi");
      Serial.println("ESP8266 in sleep mode\n");
      ESP.deepSleep(60e6);
      //break;    
    } 
    delay(5000); 
  }
   Serial.print("\nConnected, IP address: ");
   Serial.println(WiFi.localIP()); 
 }


void loop() {
  
  // Connect to WiFi, if not possible go to sleep for a while to not drain battery.
  {
  connectWifiIfNotConnected();
  delay(100);
  }
  
  // Writing BME280 data to serial
  {
   printBME280Data(&Serial);
   delay(100);
  }

  //Writing Battery Voltage to serial
  {
  printBatMonData(&Serial);
  delay(100);
  }
  
  // ThingSpeak flow
  
  ThingSpeak.setField(1, int(temp));
  ThingSpeak.setField(2, int(hum));
  ThingSpeak.setField(3, int(pres) / 100);
  ThingSpeak.setField(4, int(seaLevel));
  ThingSpeak.setField(5, int(dewPoint));
  ThingSpeak.setField(6, int(absoluteHumidity));
  ThingSpeak.setField(7, volt);
  
  // set the status
  String myStatus = ("Active, Battery Voltage: " + String(volt) + "V");
  ThingSpeak.setStatus(myStatus);
  
  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  
  //Blink
  //{
  //digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level)
  //delay(500);                      // Wait for a second
  //digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  //delay(0);                      // Wait for two seconds (to demonstrate the active low LED)
  //}
  
  //delay(0); // Wait 60 seconds to update the channel again 
  
  //Sleep
  Serial.println("ESP8266 in sleep mode\n");
  ESP.deepSleep(300e6); // 5 minutes sleep   
}
