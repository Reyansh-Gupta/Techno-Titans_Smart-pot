#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <DHT.h>
#include <Servo.h>

// ====================== PINS ======================
#define TFT_CS     10
#define TFT_RST     8
#define TFT_DC      9
#define DHTPIN      2
#define DHTTYPE     DHT11
#define SOIL_PIN    A0
#define WATER_SENSE_PIN 4  
#define BUZZER_PIN  6
#define RED_LED     7
#define GREEN_LED   12
#define SERVO_PIN   5

// ====================== OBJECTS ======================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);
Servo wateringServo;

// ====================== VARIABLES ======================
int soilPercent = 0;
float temperature = 24.0; // Startup default
float humidity = 50.0;    // Startup default
bool tankEmpty = false;

unsigned long lastWaterTime = 0;
int lastSoilBefore = 0;
int lastSoilAfter = 0;
bool hasWateredOnce = false;

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  // Set to INPUT (not pullup) to allow the sensor's own board to 
  // pull the signal HIGH when it detects a "Dry" or "Low Water" state.
  pinMode(WATER_SENSE_PIN, INPUT); 

  wateringServo.attach(SERVO_PIN);
  wateringServo.write(0);
  delay(500);
  wateringServo.detach(); 

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
  
  dht.begin();
  
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(25, 50);
  tft.print("SMART POT"); 
  tft.setTextSize(1);
  tft.setCursor(45, 80);
  tft.print("SYSTEM READY");
  delay(2000);
  tft.fillScreen(ST7735_BLACK);
}

void loop() {
  readSensors();
  checkTankAndAlarm(); // Full screen red alert if low water
  
  if (!tankEmpty) {
    displayStatus();
    // Smart Logic: Water if soil < 45%
    if (soilPercent < 45) {
      smartWaterPlant();
    } else {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  delay(1000); 
}

void readSensors() {
  // Soil mapping: 1023 (Dry) to 200 (Wet)
  soilPercent = constrain(map(analogRead(SOIL_PIN), 1023, 200, 0, 100), 0, 100);

  // DHT11 Reliability Check
  float t_raw = dht.readTemperature();
  float h_raw = dht.readHumidity();
  
  // Only update if reading is valid, otherwise keep previous value
  if (!isnan(t_raw) && t_raw > 0) {
    temperature = t_raw;
    humidity = h_raw;
  }
}

void displayStatus() {
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.setCursor(10, 5);
  tft.print("Plant Status");

  tft.setCursor(10, 35);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.print("Soil: ");
  tft.setTextColor(soilPercent < 40 ? ST7735_RED : ST7735_GREEN, ST7735_BLACK);
  tft.print(soilPercent);
  tft.print("%  ");

  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(10, 65);
  tft.print("T: "); tft.print(temperature, 1); tft.print("C  ");
  tft.setCursor(10, 90);
  tft.print("H: "); tft.print(humidity, 1); tft.print("%  ");

  // LAST WATERED LOGIC
  if (hasWateredOnce) {
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    tft.setCursor(10, 120);
    unsigned long secondsAgo = (millis() - lastWaterTime) / 1000;
    tft.print("Last: ");
    if (secondsAgo < 60) { tft.print(secondsAgo); tft.print("s ago   "); }
    else { tft.print(secondsAgo / 60); tft.print("m ago   "); }

    tft.setCursor(10, 135);
    tft.print("Change: ");
    tft.print(lastSoilBefore); tft.print("% -> "); tft.print(lastSoilAfter); tft.print("%");
  }
}

void checkTankAndAlarm() {
  // Logic: Digital HIGH on Pin 4 indicates "Water Level Low/Empty"
  tankEmpty = (digitalRead(WATER_SENSE_PIN) == HIGH); 

  if (tankEmpty) {
    digitalWrite(GREEN_LED, LOW);
    
    // FULL SCREEN ALERT
    tft.fillScreen(ST7735_RED);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(3);
    tft.setCursor(35, 40);
    tft.print("TANK");
    tft.setCursor(25, 80);
    tft.print("EMPTY!");

    // 5-SECOND RHYTHMIC ALARM
    unsigned long alarmStart = millis();
    while (millis() - alarmStart < 5000) {
      digitalWrite(RED_LED, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(250);
      digitalWrite(RED_LED, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      delay(250);
    }
    
    // Reset screen after alarm
    tft.fillScreen(ST7735_BLACK);
    tankEmpty = (digitalRead(WATER_SENSE_PIN) == HIGH);
  }
}

void smartWaterPlant() {
  lastSoilBefore = soilPercent;
  
  // Smart dynamic duration calculation
  int duration = 2000 + ((45 - soilPercent) * 65); 
  if (humidity < 35) duration += 1200; 
  duration = constrain(duration, 2000, 8500); 

  tft.setCursor(10, 150);
  tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.print("SMART WATERING...");

  wateringServo.attach(SERVO_PIN); 
  wateringServo.write(90);        
  delay(duration);          
  wateringServo.write(0);         
  delay(600);
  wateringServo.detach();         

  lastWaterTime = millis();
  hasWateredOnce = true;
  
  delay(2000); 
  readSensors();
  lastSoilAfter = soilPercent;
  tft.fillRect(0, 145, 160, 15, ST7735_BLACK); 
}
