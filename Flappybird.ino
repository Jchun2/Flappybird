#include <LiquidCrystal.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <Servo.h>
#include <EEPROM.h>

// LCD setup
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Sensor & Servo
VL53L0X sensor;
Servo flapServo;

// LED pins
const int redLED = 8;
const int greenLED = 9;

// Servo logic
int servoPos = 90;
const int servoStepForward = 15;
const int servoStepBack = 5;
const int servoWinThreshold = 150;
bool gameWon = false;

// Scoreboard
int score = 0;
int highScore = 0;

// Game elements
const int screenWidth = 16;
byte birdChar[8];
int birdPixel = 4;
unsigned long lastFrame = 0;
int frameDelay = 400;
int pipeX = screenWidth - 1;
int pipeGapY = 6;
int pipeGapSize = 4;
bool passedPipe = false;
int lastDist = 0;
const int resetThreshold = 50;

// Pipe arrays
byte pipeTop[8];
byte pipeBottom[8];

void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  Wire.begin();

  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  digitalWrite(redLED, LOW);
  digitalWrite(greenLED, LOW);

  flapServo.attach(10);
  flapServo.write(servoPos);

  highScore = EEPROM.read(0);
  if (highScore > 250) highScore = 0;

  lcd.clear();
  lcd.print("INIT SENSOR...");
  delay(500);

  sensor.init();
  sensor.setTimeout(500);
  sensor.startContinuous();

  lcd.clear();
}

void loop() {
  int dist = sensor.readRangeContinuousMillimeters();

  if (gameWon) {
    if (!sensor.timeoutOccurred()) {
      int diff = abs(dist - lastDist);
      lastDist = dist;
      if (diff > resetThreshold) {
        resetGame();
      }
    }
    return;
  }

  if (millis() - lastFrame < frameDelay) return;
  lastFrame = millis();

  if (!sensor.timeoutOccurred()) {
    dist = constrain(dist, 50, 300);
    birdPixel = map(dist, 300, 50, 0, 15);
    lastDist = dist;
  }

  pipeX--;
  if (pipeX < 0) {
    pipeX = screenWidth - 1;

    // 🌟 UPDATED PIPE GAP LOGIC
    const int minGap = 3;
    const int maxGap = 5;
    pipeGapSize = random(minGap, maxGap + 1);  // random gap size
    pipeGapY = random(2, 13 - pipeGapSize);    // random position for the gap

    int topPipeHeight = pipeGapY;
    int bottomPipeHeight = 16 - (pipeGapY + pipeGapSize);

    createPipeBytes(topPipeHeight, bottomPipeHeight);

    passedPipe = false;
  }

  lcd.clear();
  for (int col = 0; col < screenWidth; col++) {
    if (col == pipeX) {
      lcd.setCursor(col, 0);
      lcd.write(byte(1));
      lcd.setCursor(col, 1);
      lcd.write(byte(2));
    }
  }

  int birdRow = (birdPixel < 8) ? 0 : 1;
  int birdOffset = birdPixel % 8;
  makeBirdChar(birdOffset);
  lcd.createChar(3, birdChar);
  lcd.setCursor(4, birdRow);
  lcd.write(byte(3));

  if (pipeX == 4) {
    if (birdPixel < pipeGapY || birdPixel > (pipeGapY + pipeGapSize - 1)) {
      if(score > 0) { score--; }
      else { score = 0; }
      servoPos -= servoStepBack;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      digitalWrite(redLED, HIGH);
      delay(100);
      digitalWrite(redLED, LOW);
    } 
    else { 
      score++;
      servoPos += servoStepForward;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      digitalWrite(greenLED, HIGH);
      delay(100);
      digitalWrite(greenLED, LOW);

      if (servoPos >= servoWinThreshold && !gameWon) {
        winGame();
      }
    }
  }

  adjustFrameDelay();
}

void createPipeBytes(int topHeight, int bottomHeight) {
  for (int i = 0; i < 8; i++) {
    pipeTop[i] = B00000;
    pipeBottom[i] = B00000;
  }

  for (int i = 0; i < 8; i++) {
    if (i < topHeight && i < 8) {
      pipeTop[i] = B11111;
    }
    if (i >= 8 - bottomHeight && i < 8) {
      pipeBottom[i] = B11111;
    }
  }

  lcd.createChar(1, pipeTop);
  lcd.createChar(2, pipeBottom);
}

void makeBirdChar(int pixelY) {
  for (int i = 0; i < 8; i++) {
    birdChar[i] = (i == pixelY || i == pixelY + 1) ? B00100 : B00000;
  }
}

void winGame() {
  gameWon = true;
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("YOU WIN!");
  lcd.setCursor(1, 1);
  lcd.print("Servo: ");
  lcd.print(servoPos);
  flapServo.write(servoPos);
}

void resetGame() {
  score = 0;
  gameWon = false;
  pipeX = screenWidth - 1;
  pipeGapY = random(2, 10);
  birdPixel = 4;
  passedPipe = false;
  servoPos = 90;
  flapServo.write(servoPos);
  digitalWrite(redLED, LOW);
  lcd.clear();
  lastFrame = millis();
}

void adjustFrameDelay() {
  if (score >= 5) {
    frameDelay = max(200, frameDelay - 10);
  }
  if (score >= 10) {
    frameDelay = max(150, frameDelay - 15);
  }
  if (score >= 15) {
    frameDelay = max(100, frameDelay - 20);
  }
}
