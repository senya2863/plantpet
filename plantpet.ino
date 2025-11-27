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

//акселерометр
#define BMI160_I2C_ADDR 0x69

void setup() {
  Serial.begin(115200);
  delay(5000); 

  //soil moisture
  pinMode(SENSOR_POWER, OUTPUT);
  digitalWrite(SENSOR_POWER, LOW);

  //temperature
  tempSensor.begin();

  //акселерометр
  Wire.begin(D2, D1); //SDA, SCL
  initBMI160();
  delay(100);
}

void initBMI160() {
  // Сначала сброс
  writeRegister(0x7E, 0xB6);
  delay(100);
  
  // Запуск акселерометра
  writeRegister(0x7E, 0x11);
  delay(100);
  
  // Настройка акселерометра
  writeRegister(0x40, 0x2B); // normal mode, 100Hz
  writeRegister(0x41, 0x01); // ±4g range
}

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BMI160_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t read16(uint8_t reg) {
  Wire.beginTransmission(BMI160_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI160_I2C_ADDR, 2);
  
  // Проверяем доступность данных
  if (Wire.available() < 2) {
    return 0;
  }
  
  uint8_t low = Wire.read();
  uint8_t high = Wire.read();
  return (high << 8) | low;
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

//func for акселерометр motion detection
String getMotionStatus() {
  // Чтение данных акселерометра
  int16_t ax = read16(0x12);
  int16_t ay = read16(0x14);
  int16_t az = read16(0x16);
  
  static int16_t last_ax = 0, last_ay = 0, last_az = 0;
  int delta = abs(ax - last_ax) + abs(ay - last_ay) + abs(az - last_az);
  last_ax = ax; last_ay = ay; last_az = az;

  if (delta > 1000) {  // Порог изменения
      return "MOTION";
  } else {
      return "STABLE";
  }
}


void loop() {
  //moisture level
  int moisture = readMoisture();
  int moisturePercent = percentMoisture(moisture);
  String moistureStatus = getMoistureStatus(moisturePercent);
  
  //temperature
  tempSensor.requestTemperatures();
  float temperature = tempSensor.getTempCByIndex(0);
  String tempStatus = getTemperatureStatus(temperature);
  
  //motion
  String motionStatus = getMotionStatus();
  
  //serial output
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.print(" (");
  Serial.print(moisturePercent);
  Serial.print("%) | Temp: ");
  Serial.print(temperature);
  Serial.print("C | Motion: ");
  Serial.println(motionStatus);

  delay(2000);
}