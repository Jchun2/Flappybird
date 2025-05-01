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
bool passedPipe = false;
int lastDist = 0;
const int resetThreshold = 50;

// Pipe arrays (to be updated dynamically)
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


//  // Display score
//   int scoreCol = 16 - String(score).length();  // +2 for "S:"
//   lcd.setCursor(scoreCol, 0);
//   lcd.print(score);

  // Move pipe
  pipeX--;
  if (pipeX < 0) {
    pipeX = screenWidth - 1;

    // Randomize pipe heights (top and bottom)
    int topPipeHeight = random(1, 5); // Random height for top pipe (1 to 4)
    int bottomPipeHeight = random(5, 9); // Random height for bottom pipe (5 to 8)

    // Define the gap position based on the top pipe's height
    pipeGapY = random(topPipeHeight + 1, bottomPipeHeight - 1); // Ensures a valid gap

    // Create dynamic byte arrays for top and bottom pipes
    createPipeBytes(topPipeHeight, bottomPipeHeight);

    passedPipe = false;
  }



  // Draw scene
  lcd.clear();
  for (int col = 0; col < screenWidth; col++) {
    if (col == pipeX) {
      // Draw top pipe using dynamically created byte array
      lcd.setCursor(col, 0);
      lcd.write(byte(1));

      // Draw bottom pipe using dynamically created byte array
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
      if(score>0) {score--;}
      else {score=0;}
      servoPos -= servoStepBack;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      digitalWrite(redLED, HIGH);
      delay(100);
      digitalWrite(redLED, LOW);
      
    } else { // Success: servo step forward

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
  // Reset pipe arrays
  for (int i = 0; i < 8; i++) {
    pipeTop[i] = B00000;
    pipeBottom[i] = B00000;
  }

  // Set the top pipe based on the random height
  for (int i = 0; i < topHeight; i++) {
    pipeTop[i] = B11111; // Fill top pipe part with ones (active pixels)
  }

  // Set the bottom pipe based on the random height
  for (int i = 8 - bottomHeight; i < 8; i++) {
    pipeBottom[i] = B11111; // Fill bottom pipe part with ones (active pixels)
  }

  // Create custom LCD characters for pipe top and bottom
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
    frameDelay = max(200, frameDelay - 10);  // Decrease frameDelay by 10ms for every 10 points, but not below 200ms
  }
  if (score >= 10) {
    frameDelay = max(150, frameDelay - 15);  // Decrease frameDelay further for higher score, but not below 150ms
  }
  if (score >= 15) {
    frameDelay = max(100, frameDelay - 20);  // Decrease more at higher score, but not below 100ms
  }
}
