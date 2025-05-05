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

// Variable for threshold (starting distance for the bird)
int threshold = 0;

void waitForHandToStart() {
  const int triggerDistance = 500;  // mm — hand must be closer than this
  const int requiredDuration = 3000; // 3 seconds in ms
  const int barLength = 16;
  const int updateInterval = 100;

  unsigned long handStartTime = 0;
  bool handPresent = false;
  int progress = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("To start, hand");
  lcd.setCursor(0, 1);
  lcd.print("above the sensor");
  delay(7000);

  // the hand has to be over the sensor for 3 seconds to start the game
  while (true) {
    int dist = sensor.readRangeContinuousMillimeters();

    if (!sensor.timeoutOccurred() && dist < triggerDistance) {
      if (!handPresent) {
        handStartTime = millis(); // ms since arduino began program
        handPresent = true;
      }

      unsigned long heldTime = millis() - handStartTime;
      progress = map(heldTime, 0, requiredDuration, 0, barLength);
      progress = constrain(progress, 0, barLength);

      // Draw progress bar
      lcd.setCursor(0, 1);
      for (int i = 0; i < barLength; i++) {
        if (i < progress) lcd.print((char)255);  // Full blocks
        else lcd.print(" ");
      }

      if (heldTime >= requiredDuration) break;
    } else {
      // Reset if hand removed
      handPresent = false;
      progress = 0;
      lcd.setCursor(0, 1);
      lcd.print("                "); // Clear bar
    }

    delay(updateInterval);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Put your hand");
  lcd.setCursor(0, 1);
  lcd.print("at wanted height");
  delay(3000);
  lcd.clear();
}


void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  Wire.begin();

  pinMode(greenLED, OUTPUT);
  digitalWrite(greenLED, LOW);

  flapServo.attach(10);
  flapServo.write(servoPos);
  // code if we wanted to implement a high score system
  // highScore = EEPROM.read(0);
  // if (highScore > 250) highScore = 0;

  sensor.init();
  sensor.setTimeout(500);
  sensor.startContinuous();

  waitForHandToStart();  
  threshold = countdownAndGetDistance();  // Continue setup

  lcd.clear(); // Clear the LCD after countdown
}


int countdownAndGetDistance() {
  long totalDistance = 0;
  int readings = 3; // We want to take 3 readings to average as the threshold for movement
  lcd.setCursor(0, 0);
  lcd.write("Stay still");
  lcd.setCursor(0, 1);
  lcd.write("for set up");
  delay(3000);
  for (int i = 3; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write("Stay still");
    lcd.setCursor(7, 1); 
    lcd.print(i);       
    delay(1000);         

    // Take a reading from the sensor and accumulate it
    int distance = getSensorReading();
    totalDistance += distance;  

    lcd.setCursor(7, 1);  
    lcd.print(" ");       
  }

  // Calculate the average distance
  int averageDistance = totalDistance / readings;
  lcd.setCursor(7, 1);  
  lcd.print("GO!");     
  delay(500);        

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

  // if they won the game, they have the option to play again
  if (gameWon) {
    static unsigned long handStartTime = 0;
    int dist = getSensorReading();

    if (!sensor.timeoutOccurred() && dist < 500) {  // Hand detected
      if (handStartTime == 0) {
        handStartTime = millis();  // Start timer
      } else if (millis() - handStartTime >= 3000) {
        resetGame();  // Held for 3 seconds
        handStartTime = 0;  // Reset timer
      }
    } else {
      handStartTime = 0;  // Reset if hand removed
    }

    return;
  }

  if (millis() - lastFrame < frameDelay) return; // so frame is not drawn too early
  lastFrame = millis();

  if (!sensor.timeoutOccurred()) {
    const int movementRange = 120; // total range around threshold
    const int deadZone = 10;       // ignore movement within ±10 mm

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

    const int minGap = 2;
    const int maxGap = 6;

    int effectiveScore = constrain(score, -4, 4);
    pipeGapSize = map(effectiveScore, -4, 4, maxGap, minGap);  // Higher score = smaller gap

    pipeGapY = random(1, 16 - pipeGapSize - 1);  // Ensure gap fits on screen

    int topPipeHeight = pipeGapY; 
    int bottomPipeHeight = 16 - (pipeGapY + pipeGapSize); 

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
    lcd.createChar(1, pipeTop);
    lcd.createChar(2, pipeBottom);
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

  // score mechanism
  if (pipeX == 4) {
    if (birdPixel < pipeGapY || birdPixel > (pipeGapY + pipeGapSize - 1)) {
      if (score > 0) {
        score--;
        frameDelay += 50;
      } else {
        score = 0;
      }
      servoPos -= servoStepBack;
      servoPos = constrain(servoPos, 0, 180);
      flapServo.write(servoPos);

      digitalWrite(redLED, HIGH);
      delay(100);
      digitalWrite(redLED, LOW);
    } else {
      score++;
      frameDelay -= 50;
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
    byte base = mergeWithPipe ? pipeData[i] : B00000;

    // If this row matches the bird's vertical position, add the bird pixel
    if (i == pixelY) {
      base |= B00100; 
    }

    birdChar[i] = base;
  }
}

void winGame() {
  gameWon = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("YOU WIN! To play");
  lcd.setCursor(0, 1);
  lcd.print("again, hold 3s");
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
  frameDelay = 400;
  flapServo.write(servoPos);
  digitalWrite(redLED, LOW);
  lcd.clear();
  lastFrame = millis();
}
