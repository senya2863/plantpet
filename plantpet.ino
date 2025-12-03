#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define TFT_CS    D8
#define TFT_RST   D3
#define TFT_DC    D4
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define DS18B20_PIN D0
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);

#define SOIL_MOISTURE_PIN A0
const int DRY_VALUE = 810;
const int WET_VALUE = 308;

#define BMI160_I2C_ADDR 0x69
#define SDA_PIN D2
#define SCL_PIN D1

// Глобальные переменные состояния
unsigned long lastMotionTime = 0;
bool displayOn = true;
bool wasMoving = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSensorUpdate = 0;

// Анимация стакана воды
struct WaterGlass {
  int x, y;
  int waterLevel;
  bool visible;
};
WaterGlass waterGlass;

// Переменные для оптимизации анимации воды
static int waveOffset = 0;
static unsigned long lastWaveUpdate = 0;
static int waterAnimation = 0;
static unsigned long lastWaterGlassUpdate = 0;

// Структура для пузырьков
struct Bubble {
  int x, y;
  float size;
  float speed;
};
Bubble bubbles[30]; // Уменьшено количество пузырьков для производительности
bool bubblesInitialized = false;

// Цвета
#define BLACK    0x0000
#define WHITE    0xFFFF
#define BLUE     0x001F
#define YELLOW   0xFFE0
#define RED      0xF800
#define ORANGE   0xFD20
#define LIGHT_BLUE 0x867F
#define DARK_BLUE 0x0015
#define MEDIUM_BLUE 0x64C8
#define LIGHTEST_BLUE 0x867F

// Предварительно вычисленные цвета воды для градиента
uint16_t waterColors[3];
bool colorsInitialized = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Инициализация дисплея
  tft.init(240, 320);
  tft.setRotation(3);
  tft.fillScreen(BLACK);
  tft.invertDisplay(false);

  // Инициализация датчиков
  tempSensor.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  initBMI160();

  // Инициализация стакана воды
  waterGlass.x = 120;
  waterGlass.y = 100;
  waterGlass.waterLevel = 50;
  waterGlass.visible = false;

  // Инициализация цветов воды
  waterColors[0] = tft.color565(135, 206, 235); // Верх
  waterColors[1] = tft.color565(100, 149, 237); // Середина
  waterColors[2] = tft.color565(70, 130, 180);  // Низ
  
  colorsInitialized = true;
}

void initBMI160() {
  writeRegister(0x7E, 0xB6); delay(100);
  writeRegister(0x7E, 0x11); delay(100);
  writeRegister(0x40, 0x2B);
  writeRegister(0x41, 0x01);
}

void writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(BMI160_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int16_t read16(uint8_t reg) {
  Wire.beginTransmission(BMI160_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI160_I2C_ADDR, 2);
  if (Wire.available() < 2) return 0;
  uint8_t l = Wire.read(), h = Wire.read();
  return (h << 8) | l;
}

int readMoisture() {
  delay(5); // Уменьшено время задержки
  return analogRead(SOIL_MOISTURE_PIN);
}

int percentMoisture(int raw) {
  return constrain(map(raw, DRY_VALUE, WET_VALUE, 0, 100), 0, 100);
}

String getMotionStatus() {
  int16_t ax = read16(0x12), ay = read16(0x14), az = read16(0x16);
  static int16_t lax = 0, lay = 0, laz = 0;
  int delta = abs(ax - lax) + abs(ay - lay) + abs(az - laz);
  lax = ax; lay = ay; laz = az;

  if (delta > 800) {
    lastMotionTime = millis();
    wasMoving = true;
    return "MOVING";
  } else {
    if (millis() - lastMotionTime > 3000) {
      wasMoving = false;
    }
    return "STABLE";
  }
}

// Рисование глаз (оптимизировано)
void drawEyes(int x, int y, bool shake = false) {
  int s = shake && (millis()/100 % 2) ? random(-4,5) : 0;
  
  // Рисуем глаза одним вызовом
  tft.fillEllipse(x-50, y+s, 25, 20, WHITE);
  tft.fillEllipse(x+50, y+s, 25, 20, WHITE);
  tft.fillEllipse(x-50, y+s, 24, 19, BLACK);
  tft.fillEllipse(x+50, y+s, 24, 19, BLACK);
  
  // Зрачки
  tft.fillCircle(x-50, y+s+5, 9, WHITE);
  tft.fillCircle(x+50, y+s+5, 9, WHITE);
}

// Отрисовка градиента воды (оптимизировано)
void drawWaterGradient() {
  int height = tft.height();
  int third = height / 3;
  
  // Рисуем три полосы градиента вместо пиксельных линий
  tft.fillRect(0, 0, tft.width(), third, waterColors[0]);
  tft.fillRect(0, third, tft.width(), third, waterColors[1]);
  tft.fillRect(0, 2*third, tft.width(), height - 2*third, waterColors[2]);
}

// Отрисовка волн (оптимизировано)
void drawWaves() {
  unsigned long currentMillis = millis();
  
  // Обновляем смещение волн чаще
  if (currentMillis - lastWaveUpdate > 40) { // Быстрее обновление
    waveOffset = (waveOffset + 8) % 100; // Быстрее движение
    lastWaveUpdate = currentMillis;
  }
  
  int width = tft.width();
  int height = tft.height();
  int waveSpacing = 18; // Увеличен интервал для производительности
  
  // Рисуем только часть волн за кадр для оптимизации
  static int waveStart = 0;
  waveStart = (waveStart + 10) % waveSpacing;
  
  for(int waveY = waveStart; waveY < height; waveY += waveSpacing) {
    // Выбираем цвет волны в зависимости от глубины
    uint16_t waveColor;
    if (waveY < height / 3) {
      waveColor = tft.color565(175, 238, 238);
    } else if (waveY < 2 * height / 3) {
      waveColor = tft.color565(135, 206, 250);
    } else {
      waveColor = tft.color565(100, 149, 237);
    }
    
    // Рисуем волну линией вместо пикселей
    int prevY = waveY + sin(waveOffset * 0.08) * 5;
    for(int i = 10; i < width; i += 10) {
      float waveFreq = 0.06 + (waveY % 50) * 0.0008;
      int currentY = waveY + sin((i + waveOffset + waveY) * waveFreq) * 
                    (6 + sin((waveY + currentMillis/800.0) * 0.08) * 2);
      
      if (currentY >= 0 && currentY < height) {
        tft.drawLine(i-10, prevY, i, currentY, waveColor);
        prevY = currentY;
      }
    }
  }
}

// Инициализация пузырьков
void initBubbles() {
  for(int i = 0; i < 30; i++) {
    bubbles[i].x = random(tft.width());
    bubbles[i].y = random(tft.height());
    bubbles[i].size = random(2, 6); // Уменьшен максимальный размер
    bubbles[i].speed = random(8, 15) / 10.0; // Более узкий диапазон скоростей
  }
  bubblesInitialized = true;
}

// Обновление и отрисовка пузырьков (оптимизировано)
void drawBubbles() {
  unsigned long currentMillis = millis();
  static unsigned long lastBubbleUpdate = 0;
  
  if (!bubblesInitialized) {
    initBubbles();
  }
  
  // Обновляем пузырьки каждые 30мс (чаще)
  if (currentMillis - lastBubbleUpdate > 30) {
    for(int i = 0; i < 30; i++) {
      // Двигаем пузырьки
      bubbles[i].y -= bubbles[i].speed;
      
      // Если пузырек ушел за верх, респавним внизу
      if (bubbles[i].y < -10) {
        bubbles[i].x = random(tft.width());
        bubbles[i].y = tft.height() + 5;
        bubbles[i].size = random(2, 6);
        bubbles[i].speed = random(8, 15) / 10.0;
      }
      
      // Рисуем пузырек
      int radius = bubbles[i].size;
      int bx = bubbles[i].x;
      int by = bubbles[i].y;
      
      // Только если пузырек виден на экране
      if (by >= -radius && by < tft.height() + radius) {
        // Белая обводка
        tft.drawCircle(bx, by, radius, WHITE);
        
        // Полупрозрачная заливка
        uint16_t bubbleColor = tft.color565(200, 228, 255);
        tft.fillCircle(bx, by, radius - 1, bubbleColor);
        
        // Блик
        tft.fillCircle(bx - radius/3, by - radius/3, max(1, radius/3), WHITE);
      }
    }
    lastBubbleUpdate = currentMillis;
  }
}

// Рисование глаз в воде (оптимизировано)
void drawEyesInWater(int x, int y) {
  // Рисуем градиент воды
  drawWaterGradient();
  
  // Рисуем волны
  drawWaves();
  
  // Рисуем пузырьки
  drawBubbles();
  
  // Рисуем глаза поверх
  drawEyes(x, y);
}

// Огонь (оптимизировано)
void drawFireAnimation(int x, int y) {
  unsigned long currentMillis = millis();
  
  // Огонь обновляется с разной скоростью для плавности
  static unsigned long lastFireUpdate = 0;
  if (currentMillis - lastFireUpdate > 50) {
    lastFireUpdate = currentMillis;
    
    // Очищаем предыдущий кадр
    tft.fillRect(x-50, y+70, 100, 50, BLACK);
    
    // Рисуем пламя
    for(int i=-35; i<=35; i+=14) { // Увеличено расстояние между языками пламени
      int h = 25 + sin(i*0.15 + currentMillis*0.015)*16; // Быстрее анимация
      
      // Градиент пламени
      for(int j=0; j<h; j++) {
        uint16_t col;
        int level = j * 100 / h;
        
        if (level < 40) col = ORANGE;
        else if (level < 80) col = RED;
        else col = YELLOW;
        
        int w = map(j, 0, h, 8, 1);
        tft.drawFastHLine(x+i-w, y+70+j, w*2, col);
      }
    }
  }
  
  // Искры (реже появляются)
  if (random(100) > 85) {
    int sx = x + random(-25, 25);
    int sy = y + 70 + random(0, 25);
    tft.fillCircle(sx, sy, random(2,3), YELLOW);
  }
}

// Холодный рот
void drawColdMouth(int x, int y) {
  int shake = (millis()/100 % 2) ? random(-2,3) : 0; // Меньше тряски
  
  // Рисуем верхнюю и нижнюю линии
  tft.drawFastHLine(x-20, y+shake, 41, WHITE);
  tft.drawFastHLine(x-20, y+12+shake, 41, WHITE);
  
  // Пар появляется реже
  if (millis()/150 % 3 == 0) { // Реже обновляем пар
    for(int i=-12; i<=12; i+=6) // Реже линии пара
      tft.drawFastVLine(x+i, y+shake+2, 8, WHITE);
  }
}

// Рисование стакана воды (оптимизировано)
void drawWaterGlass(bool isDry) {
  unsigned long currentMillis = millis();
  
  if (isDry) {
    waterGlass.visible = true;
    
    // Обновляем анимацию воды каждые 150мс
    if (currentMillis - lastWaterGlassUpdate > 150) {
      waterAnimation = (waterAnimation + 1) % 4;
      lastWaterGlassUpdate = currentMillis;
    }
    
    int glassWidth = 60;
    int glassHeight = 100;
    int left = waterGlass.x - glassWidth/2;
    int top = waterGlass.y - glassHeight/2;
    
    // Стакан (контур)
    tft.drawRect(left, top, glassWidth, glassHeight, WHITE);
    
    // Вода в стакане
    int waterTop = waterGlass.y + glassHeight/2 - waterGlass.waterLevel;
    int waterAnimOffset = (waterAnimation == 1) ? -1 : (waterAnimation == 3) ? 1 : 0;
    
    // Заливка воды одним прямоугольником с градиентом
    int waterRectHeight = waterGlass.waterLevel;
    tft.fillRect(left + 1, waterTop, glassWidth - 2, waterRectHeight, BLUE);
    
    // Поверхность воды (простая волнистая линия)
    for(int i = 5; i < glassWidth - 5; i += 5) {
      int wave = sin((i + currentMillis * 0.01) * 0.4) * 1;
      tft.drawPixel(left + i, waterTop + wave, LIGHT_BLUE);
    }
    
    // Редкие капли
    if (random(100) > 95) {
      int dropX = left + random(2, glassWidth - 2);
      int dropY = waterTop + random(10, waterGlass.waterLevel - 10);
      tft.drawFastVLine(dropX, dropY, 2, LIGHT_BLUE);
    }
    
    // Текст "WATER"
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    tft.setCursor(waterGlass.x - 15, waterGlass.y + glassHeight/2 + 5);
    tft.print("WATER");
    
  } else if (waterGlass.visible) {
    // Если стакан был виден, но теперь не нужен - очищаем его область
    tft.fillRect(waterGlass.x - 25, waterGlass.y - 45, 60, 100, BLACK);
    waterGlass.visible = false;
  }
}

void drawHappyMouth(int x, int y) {
  // Оптимизированная улыбка
  tft.drawFastHLine(x-30, y, 61, WHITE);
  for(int i=-25; i<=25; i+=10) {
    int offset = (625 - i*i) / 100;
    tft.drawPixel(x+i, y+offset, WHITE);
    tft.drawPixel(x+i, y+offset+1, WHITE);
  }
}

void drawSadMouth(int x, int y) {
  // Оптимизированная грустная улыбка
  for(int i=-20; i<=20; i+=8) {
    int offset = (20-abs(i))/3;
    tft.drawFastHLine(x+i-2, y+offset, 5, WHITE);
  }
}

// Основная функция отрисовки с оптимизированным обновлением
void displayData(float temp, int moist, String motion) {
  unsigned long currentMillis = millis();
  
  // Обновляем дисплей каждые 33мс (~30 FPS) если есть движение
  // Или каждые 100мс (~10 FPS) если нет движения
  unsigned long updateInterval = (motion == "MOVING") ? 33 : 100;
  
  if (currentMillis - lastDisplayUpdate < updateInterval) {
    return; // Пропускаем кадр для достижения нужной частоты обновления
  }
  
  lastDisplayUpdate = currentMillis;
  
  if (!displayOn) {
    tft.fillScreen(BLACK);
    return;
  }
  
  int cx = tft.width()/2;
  int cy = tft.height()/2 - 20;
  
  bool isWet = (moist > 70);
  bool isHot = (temp > 30);
  bool isCold = (temp < 12);
  bool isDry = (moist < 40);
  bool isMoving = (motion == "MOVING");

  // Очищаем только часть экрана если возможно
  if (!isWet) {
    tft.fillRect(0, 0, tft.width(), tft.height() - 80, BLACK);
  }
  
  // Очищаем область под текстом
  tft.fillRect(0, tft.height() - 80, tft.width(), 80, BLACK);
  
  if (isMoving) {
    if (isWet) {
      drawEyesInWater(cx, cy);
    }
    else if (isHot) {
      drawEyes(cx, cy, true);
      drawFireAnimation(cx, cy);
    }
    else if (isCold) {
      drawEyes(cx, cy, true);
      drawColdMouth(cx, cy+60);
    }
    else if (isDry) {
      drawEyes(cx, cy);
      drawSadMouth(cx, cy+65);
      drawWaterGlass(true);
    }
    else {
      drawEyes(cx, cy);
      drawHappyMouth(cx, cy+60);
    }
  }
  else {
    if (isWet) {
      drawEyesInWater(cx, cy);
    }
    else if (isHot) {
      drawEyes(cx, cy, true);
      drawFireAnimation(cx, cy);
    }
    else if (isCold) {
      drawEyes(cx, cy, true);
      drawColdMouth(cx, cy+60);
    }
    else if (isDry) {
      drawEyes(cx, cy);
      drawSadMouth(cx, cy+65);
      drawWaterGlass(true);
    }
    else {
      drawEyes(cx, cy);
      tft.drawFastHLine(cx-20, cy+60, 41, WHITE);
    }
  }

  // Текст (обновляем всегда)
  tft.setTextSize(2);
  tft.setTextColor(BLUE);
  tft.setCursor(10, tft.height()-70); 
  tft.print("T:"); tft.print(temp,1); tft.print("C");
  
  tft.setCursor(10, tft.height()-40); 
  tft.print("H:"); tft.print(moist); tft.print("%");
  
  tft.setCursor(10, tft.height()-10); 
  tft.print(motion);

  tft.setCursor(tft.width()-100, tft.height()-40);
  if (isWet)      { tft.setTextColor(BLUE);   tft.print("WET!"); }
  else if (isHot) { tft.setTextColor(RED);    tft.print("HOT!"); }
  else if (isCold){ tft.setTextColor(WHITE);  tft.print("COLD"); }
  else if (isDry) { tft.setTextColor(LIGHT_BLUE); tft.print("DRY!"); }
  else            { tft.setTextColor(WHITE);  tft.print("OK"); }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Читаем датчики каждые 200мс вместо постоянного чтения
  static float temp = 20.0;
  static int moist = 50;
  
  if (currentMillis - lastSensorUpdate > 200) {
    int raw = readMoisture();
    moist = percentMoisture(raw);
    tempSensor.requestTemperatures();
    temp = tempSensor.getTempCByIndex(0);
    lastSensorUpdate = currentMillis;
  }
  
  String motion = getMotionStatus();
  
  displayData(temp, moist, motion);
  
  // Короткая задержка для стабильности
  delay(5);
}