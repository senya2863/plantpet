#include <Wire.h>     
#include <Adafruit_GFX.h> //display
#include <Adafruit_SSD1306.h> //termopara
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>


//display
Adafruit_SSD1306 display(128,64,&Wire,-1);

//temperature
#define DS18B20_PIN D4
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);

//soil moisture

#define SOIL_MOISTURE_PIN A0
#define SENSOR_POWER D1

const int DRY_VALUE = 810;  //in air
const int WET_VALUE = 308; //in water

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //soil moisture
  pinMode(SENSOR_POWER, OUTPUT);
  digitalWrite(SENSOR_POWER, LOW);

  //temperature
  tempSensor.begin();

}

//func for reading moisture
int readMoisture(){
  digitalWrite(SENSOR_POWER,HIGH); //turn on sensor
  delay(100); //stability

  int sensorValue = analogRead(SOIL_MOISTURE_PIN);
  
  digitalWrite(SENSOR_POWER, LOW); //turn off

  return sensorValue;
}

//func for moisture in percent

int percentMoisture (int raw_value){
  int moisture_percent = map(raw_value, DRY_VALUE, WET_VALUE, 0, 100);

  moisture_percent = constrain(moisture_percent, 0 , 100); //ограничение

  return moisture_percent;
}

//func for text status
String getMoistureStatus (int percent){
  if (percent<20){
    return "very wet";
  }
  else if (percent<40){
    return "wet";
  }
  else if (percent<60){
    return "normal";
  }
  else if (percent<80){
    return "dry";
  }
  else{
    return "very dry";
  }
}

//func for temperature status
String getTemperatureStatus(float temp){
  if (temp < 10){
    return "TOO COLD";
  }
  else if (temp < 18){
    return "COLD";
  }
  else if (temp < 28){
    return "NORMAL";
  }
  else if (temp < 35){
    return "WARM";
  }
  else{
    return "TOO HOT";
  }
}


void loop() {
  // put your main code here, to run repeatedly:

  //moisture level
  int moisture = readMoisture();
  Serial.print("Moisture: ");
  Serial.print(moisture);

  //temperature
  tempSensor.requestTemperatures();
  float temperature = tempSensor.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print('\n');

  delay(2000);
}