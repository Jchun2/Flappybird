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
//const int redLED = 8;
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

// Variable for threshold (starting distance for the bird)
int threshold = 0;

void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  Wire.begin();

  pinMode(greenLED, OUTPUT);
  digitalWrite(greenLED, LOW);

  flapServo.attach(10);
  flapServo.write(servoPos);

  highScore = EEPROM.read(0);
  if (highScore > 250) highScore = 0;

  lcd.clear();
  lcd.print("Have hand above sensor");

  sensor.init();
  sensor.setTimeout(500);
  sensor.startContinuous();

  threshold = countdownAndGetDistance();

  lcd.clear(); // Clear the LCD after countdown
}


int countdownAndGetDistance() {
  long totalDistance = 0;
  int readings = 3; // We want to take 3 readings (for 3, 2, 1)

  for (int i = 3; i > 0; i--) {
    lcd.setCursor(7, 1);  // Position where the countdown is shown
    lcd.print(i);         // Display the number
    delay(1000);          // Wait for 1 second

    // Take a reading from the sensor and accumulate it
    int distance = getSensorReading();
    totalDistance += distance;  // Add the distance to the total

    lcd.setCursor(7, 1);  // Clear the countdown number
    lcd.print(" ");       // Clear the number
  }

  // Calculate the average distance
  int averageDistance = totalDistance / readings;
  lcd.setCursor(7, 1);  // Clear the final space
  lcd.print("GO!");     // Indicate the game is starting
  delay(500);           // Pause before starting the game

  return averageDistance; // Return the average distance
}

int getSensorReading() {
  int distance = sensor.readRangeContinuousMillimeters();
  // Handle potential timeout and return a safe default value
  if (sensor.timeoutOccurred()) {
    return 200; // Return a fallback value
  }
  return distance;
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
    const int movementRange = 120; // total range around threshold (e.g. ±60 mm)
    const int deadZone = 10;       // ignore jitter within ±10 mm

    int topDist = threshold - movementRange / 2;
    int bottomDist = threshold + movementRange / 2;

    // Clamp the reading
    dist = constrain(dist, topDist, bottomDist);

    // Compute offset from center
    int delta = dist - threshold;

    // Apply dead zone
    if (abs(delta) < deadZone) {
      delta = 0;
    }

    // Map to screen pixel range
    birdPixel = map(delta, movementRange / 2, -movementRange / 2, 0, 15);

    lastDist = dist;
  }

  pipeX--;
  if (pipeX < 0) {
    pipeX = screenWidth - 1;

    const int minGap = 3;
    const int maxGap = 5;

    pipeGapSize = random(minGap, maxGap + 1);
    pipeGapY = random(1, 16 - pipeGapSize - 1); // full vertical space

    int topPipeHeight = pipeGapY; // could be > 8
    int bottomPipeHeight = 16 - (pipeGapY + pipeGapSize); // could be > 8

    createPipeBytes(topPipeHeight, bottomPipeHeight);
    passedPipe = false;
  }


  lcd.clear();
  for (int col = 0; col < screenWidth; col++) {
    if (col == pipeX && col != 4) {  // Draw pipes normally unless overlapping with bird
      lcd.setCursor(col, 0);
      lcd.write(byte(1));
      lcd.setCursor(col, 1);
      lcd.write(byte(2));
    }
  }

  int birdRow = (birdPixel < 8) ? 0 : 1;
  int birdOffset = birdPixel % 8;

  if (pipeX == 4) {
    // Merge bird + pipe in bird row
    byte* pipeData = (birdRow == 0) ? pipeTop : pipeBottom;
    makeBirdChar(birdOffset, true, pipeData);
    lcd.createChar(3, birdChar);
    
    // Bird row: draw merged char
    lcd.setCursor(4, birdRow);
    lcd.write(byte(3));
    
    // Other row: draw normal pipe
    lcd.setCursor(4, 1 - birdRow);
    lcd.write((1 - birdRow == 0) ? byte(1) : byte(2));
  } else {
    // Not in same column — draw just the bird
    byte emptyPipe[8] = {0};
    makeBirdChar(birdOffset, false, emptyPipe);
    lcd.createChar(3, birdChar);
    lcd.setCursor(4, birdRow);
    lcd.write(byte(3));
  }

  if (pipeX == 4) {
    if (birdPixel < pipeGapY || birdPixel > (pipeGapY + pipeGapSize - 1)) {
      if (score > 0) {
        score--;
      } else {
        score = 0;
      }
      servoPos -= servoStepBack;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      //digitalWrite(redLED, HIGH);
      delay(100);
      //digitalWrite(redLED, LOW);
    } else {
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
  // Clear old pipe data
  for (int i = 0; i < 8; i++) {
    pipeTop[i] = B00000;
    pipeBottom[i] = B00000;
  }

  // Fill top pipe data
  for (int i = 0; i < 8; i++) {
    if (topHeight >= 8) {
      pipeTop[i] = B11111; // full block
    } else if (i < topHeight) {
      pipeTop[i] = B11111;
    }
  }

  // Fill bottom pipe data
  for (int i = 0; i < 8; i++) {
    if (bottomHeight >= 8) {
      pipeBottom[i] = B11111;
    } else if (i >= 8 - bottomHeight) {
      pipeBottom[i] = B11111;
    }
  }

  lcd.createChar(1, pipeTop);
  lcd.createChar(2, pipeBottom);
}

void makeBirdChar(int pixelY, bool mergeWithPipe, byte pipeData[8]) {
  for (int i = 0; i < 8; i++) {
    // Start with either pipe pixels or empty pixels
    byte base = mergeWithPipe ? pipeData[i] : B00000;

    // If this row matches the bird's vertical position, add the bird pixel
    if (i == pixelY) {
      base |= B00100;  // Bird pixel in the center column
    }

    birdChar[i] = base;
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
  //digitalWrite(redLED, LOW);
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
