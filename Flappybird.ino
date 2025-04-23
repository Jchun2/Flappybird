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
byte pipeTop3[8] = {
  B11111, B11111, B11111,
  B00000, B00000, B00000,
  B00000, B00000
};
byte pipeBottom3[8] = {
  B00000, B00000, B00000,
  B00000, B00000, B11111,
  B11111, B11111
};
byte birdChar[8];
int birdPixel = 4;
unsigned long lastFrame = 0;
int frameDelay =400;
int pipeX = screenWidth - 1;
int pipeGapY = 6;
bool passedPipe = false;
int lastDist = 0;
const int resetThreshold = 50;

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
  lcd.createChar(1, pipeTop3);
  lcd.createChar(2, pipeBottom3);
}

void loop() {
  int dist = sensor.readRangeContinuousMillimeters();

  // Allow movement reset only after win
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

  // Frame limiter
  if (millis() - lastFrame < frameDelay) return;
  lastFrame = millis();

  // Bird height from hand distance
  if (!sensor.timeoutOccurred()) {
    dist = constrain(dist, 50, 300);
    birdPixel = map(dist, 300, 50, 0, 15);
    lastDist = dist;
  }
  
  // Display score
  int scoreCol = 16 - String(score).length();  // +2 for "S:"
  lcd.setCursor(scoreCol, 0);
  lcd.print(score);

  // Move pipe
  pipeX--;
  if (pipeX < 0) {
    pipeX = screenWidth - 1;
    pipeGapY = random(2, 10);
    passedPipe = false;
  }

  // Draw scene
  lcd.clear();
  for (int col = 0; col < screenWidth; col++) {
    if (col == pipeX) {
      lcd.setCursor(col, 0);
      lcd.write(byte(1));
      lcd.setCursor(col, 1);
      lcd.write(byte(2));
    }
  }

  // Draw bird
  int birdRow = (birdPixel < 8) ? 0 : 1;
  int birdOffset = birdPixel % 8;
  makeBirdChar(birdOffset);
  lcd.createChar(3, birdChar);
  lcd.setCursor(4, birdRow);
  lcd.write(byte(3));

  // Collision / progress
  if (pipeX == 4) {
    if (birdPixel < pipeGapY || birdPixel > pipeGapY + 3) {
      // Crash: servo step back
      servoPos -= servoStepBack;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      digitalWrite(redLED, HIGH);
      delay(100);
      digitalWrite(redLED, LOW);
    } else if (!passedPipe) {
      // Success: servo step forward
      score++;
      passedPipe = true;
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

/*if (score > highScore) { // logs high scores
    highScore = score;
    EEPROM.write(0, highScore);
  }

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("GAME OVER");
  lcd.setCursor(0, 1);
  lcd.print("Score:");
  lcd.print(score);
  //lcd.print(" High:");
  //lcd.print(highScore);*/
