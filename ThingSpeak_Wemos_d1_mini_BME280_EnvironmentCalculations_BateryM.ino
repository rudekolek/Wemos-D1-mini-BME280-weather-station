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
//////////////////////////////BME280/////////////////////////////
#include <BME280I2C.h>
#include <Wire.h>

BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

// set the fields with the values
BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
BME280::PresUnit presUnit(BME280::PresUnit_Pa);

float temp(NAN), hum(NAN), pres(NAN); //Defining BME280 data

//////////////////////////////////////////////////////////////////

///////////EnvironmentCalculations////////////////////////////////
#include <EnvironmentCalculations.h>

EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
EnvironmentCalculations::TempUnit envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
float seaLevel = 0.00;
float dewPoint = 0.00;
float absoluteHumidity = 0.00;
float altitude = 104; //Meteostation elevation in meters
////////////////////////////////////////////////////////////////////////

/////////////////////////ThingSpeak///////////////////////////////
#include "ThingSpeak.h"
#include "secrets.h" // file with WIFI and ThingSpeak credentials

// Initialize our values
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;
///////////////////////////////////////////////////////////////////

/////////////////WIFI/////////////////////////////////////////////
#include <ESP8266WiFi.h>

char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiClient  client;
////////////////////////////////////////////////////////////////////

/////////////BateryMon/////////////////////////////////////////////
unsigned int a0read=0;
float volt=0.000;
///////////////////////////////////////////////////////////////////

#define SERIAL_BAUD 115200 // Set serial speed

void setup() {
  Serial.begin(SERIAL_BAUD);  // Initialize serial

  delay(500);
  
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  pinMode(A0, INPUT); //Analog PIN input for batery monitor via 220 Ohm

  WiFi.mode(WIFI_STA); 

  while(!Serial) {} // Wait

  Serial.println("\nESP8266 in normal mode\n");
  
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  Wire.begin(4,5); // Connected to SCL(GPIO5), SDA(GPIO4)

  while(!bme.begin())

// Printing BME280 sensor status to serial
  {
    Serial.println("Could not find BME280 sensor!");
    delay(0);
  }

  // bme.chipID(); // Deprecated. See chipModel().
  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }
  
}

void loop() {

  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

  // Writing to BME280 data to serial
  {
   printBME280Data(&Serial);
   delay(100);
  }
  
  {
  printBatMonData(&Serial);
  delay(0);
  }
  
  // ThingSpeak flow
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, hum);
  ThingSpeak.setField(3, pres);
  ThingSpeak.setField(4, seaLevel);
  ThingSpeak.setField(5, dewPoint);
  ThingSpeak.setField(6, absoluteHumidity);
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
  
  //Sleep
  Serial.println("ESP8266 in sleep mode\n");
  delay(0); // Wait 60 seconds to update the channel again
  
  ESP.deepSleep(300e6); // 5 minutes sleep
  
  
}

//BME 280 data to serial function
void printBME280Data
(
   Stream* client
)

{
   bme.read(pres, temp, hum, tempUnit, presUnit);
   
 //seaLevel = EnvironmentCalculations::SealevelAlitude(altitude, temp, pres); // Deprecated. See EquivalentSeaLevelPressure().
   seaLevel = EnvironmentCalculations::EquivalentSeaLevelPressure(altitude, temp, pres);
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
   client->print(pres);
   client->print(String( presUnit == BME280::PresUnit_hPa ? "hPa" :"Pa")); // expected hPa and Pa only
   client->print("\t\tEquivalent Sea Level Pressure: ");
   client->print(seaLevel);
   client->print(String( presUnit == BME280::PresUnit_hPa ? "hPa" :"Pa") + "\n"); // expected hPa and Pa only
   
   client->print("Dew point: ");
   client->print(dewPoint);
   client->print("°"+ String(tempUnit == BME280::TempUnit_Celsius ? 'C' :'F') +"\n");

   delay(0);
}

//Battery voltage calculation and print
void printBatMonData
(
   Stream* client
)

{
// put your setup code here, to run once:
 a0read = analogRead(A0);
 volt = (a0read/1023.0) * 5.265; // 5.265 Empiric number of Voltage
 client->print("Battery Voltage: ");
 client->print(volt);
 client->print("V\t\t");
 client->print(a0read);
 client->print("\n");
}
  

 
