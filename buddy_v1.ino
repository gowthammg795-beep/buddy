/*
  ================================================================
                     BUDDY V1 ROBOT
  ================================================================

  Features:
    - Manual movement
    - Automatic obstacle avoidance
    - HC-SR04 ultrasonic sensor
    - OLED animated eyes
    - OLED blinking animation
    - Manual / Auto mode

  Controller:
    ESP32

  Display:
    SSD1306 OLED 128x64

  Motor Driver:
    TB6612FNG

  Ultrasonic:
    HC-SR04

  ================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ================================================================
// OLED
// ================================================================

#define OLED_SDA 21
#define OLED_SCL 23

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);

// ================================================================
// MOTOR DRIVER - TB6612FNG
// ================================================================

// Motor A
#define PWMA 4
#define AIN1 18
#define AIN2 19

// Motor B
#define PWMB 5
#define BIN1 2
#define BIN2 15

#define STBY 12

// ================================================================
// ULTRASONIC
// ================================================================

#define TRIG_PIN 13
#define ECHO_PIN 14

// ================================================================
// MODE BUTTON
// ================================================================

#define MODE_BUTTON 32

// ================================================================
// MOTOR SPEED
// ================================================================

int motorSpeed = 180;

// ================================================================
// OBSTACLE SETTINGS
// ================================================================

#define OBSTACLE_DISTANCE 25

// ================================================================
// MODES
// ================================================================

enum RobotMode {
  MANUAL_MODE,
  AUTO_MODE
};

RobotMode currentMode = MANUAL_MODE;

// ================================================================
// MOVEMENT
// ================================================================

enum Movement {
  STOPPED,
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT
};

Movement currentMovement = STOPPED;

// ================================================================
// OLED ANIMATION
// ================================================================

unsigned long lastEyeUpdate = 0;
unsigned long lastBlink = 0;

bool eyesClosed = false;

int eyeOffset = 0;

const unsigned long EYE_UPDATE_TIME = 80;
const unsigned long BLINK_INTERVAL = 4000;

// ================================================================
// AUTO MODE TIMING
// ================================================================

unsigned long lastAutoAction = 0;

bool turning = false;

unsigned long turnStart = 0;

int turnDirection = 0;

// ================================================================
// SETUP
// ================================================================

void setup() {

  Serial.begin(115200);

  // --------------------------------------------------------------
  // OLED
  // --------------------------------------------------------------

  Wire.begin(OLED_SDA, OLED_SCL);

  oled.begin();

  oled.clearBuffer();

  oled.setFont(u8g2_font_6x10_tf);

  oled.drawStr(28, 30, "BUDDY V1");
  oled.drawStr(20, 45, "INITIALIZING");

  oled.sendBuffer();

  delay(1000);

  // --------------------------------------------------------------
  // MOTOR PINS
  // --------------------------------------------------------------

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);

  digitalWrite(STBY, HIGH);

  // --------------------------------------------------------------
  // ULTRASONIC
  // --------------------------------------------------------------

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

  // --------------------------------------------------------------
  // MODE BUTTON
  // --------------------------------------------------------------

  pinMode(MODE_BUTTON, INPUT_PULLUP);

  // --------------------------------------------------------------
  // INITIAL STATE
  // --------------------------------------------------------------

  stopMotors();

  drawEyes();

  Serial.println();
  Serial.println("==============================");
  Serial.println("       BUDDY V1 READY");
  Serial.println("==============================");
  Serial.println("MODE: MANUAL");
  Serial.println("==============================");
}

// ================================================================
// MAIN LOOP
// ================================================================

void loop() {

  handleModeButton();

  updateEyes();

  if (currentMode == MANUAL_MODE) {

    manualMode();

  } else {

    autoMode();

  }

  delay(10);
}

// ================================================================
// MODE BUTTON
// ================================================================

void handleModeButton() {

  static bool lastButtonState = HIGH;
  static unsigned long lastDebounce = 0;

  bool buttonState = digitalRead(MODE_BUTTON);

  if (buttonState != lastButtonState) {

    lastDebounce = millis();

  }

  if ((millis() - lastDebounce) > 50) {

    if (lastButtonState == HIGH && buttonState == LOW) {

      if (currentMode == MANUAL_MODE) {

        currentMode = AUTO_MODE;

        stopMotors();

        Serial.println("MODE -> AUTO");

      } else {

        currentMode = MANUAL_MODE;

        stopMotors();

        Serial.println("MODE -> MANUAL");

      }

    }
  }

  lastButtonState = buttonState;
}

// ================================================================
// MANUAL MODE
// ================================================================
//
// Serial commands:
//
// F = Forward
// B = Backward
// L = Left
// R = Right
// S = Stop
// A = Auto
// M = Manual
//
// ================================================================

void manualMode() {

  if (Serial.available()) {

    char command = Serial.read();

    command = toupper(command);

    switch (command) {

      case 'F':
        moveForward();
        break;

      case 'B':
        moveBackward();
        break;

      case 'L':
        turnLeft();
        break;

      case 'R':
        turnRight();
        break;

      case 'S':
        stopMotors();
        break;

      case 'A':

        currentMode = AUTO_MODE;

        stopMotors();

        Serial.println("MODE -> AUTO");

        break;

      case 'M':

        currentMode = MANUAL_MODE;

        stopMotors();

        Serial.println("MODE -> MANUAL");

        break;

      default:
        break;
    }
  }
}

// ================================================================
// AUTO MODE
// ================================================================

void autoMode() {

  unsigned long now = millis();

  // --------------------------------------------------------------
  // Continue an active turn
  // --------------------------------------------------------------

  if (turning) {

    if (now - turnStart < 450) {

      if (turnDirection == 1) {

        turnRight();

      } else {

        turnLeft();

      }

      return;

    }

    turning = false;

    stopMotors();

    delay(80);
  }

  // --------------------------------------------------------------
  // Read ultrasonic
  // --------------------------------------------------------------

  float distance = readDistance();

  Serial.print("AUTO DISTANCE: ");
  Serial.print(distance);
  Serial.println(" cm");

  // --------------------------------------------------------------
  // Obstacle detected
  // --------------------------------------------------------------

  if (distance > 0 && distance <= OBSTACLE_DISTANCE) {

    Serial.println("OBSTACLE DETECTED!");

    stopMotors();

    delay(120);

    // Choose a turn direction.

    // For a simple V1 robot, alternate directions.
    static bool alternateTurn = false;

    alternateTurn = !alternateTurn;

    if (alternateTurn) {

      turnDirection = 1;

      Serial.println("AUTO -> RIGHT");

    } else {

      turnDirection = -1;

      Serial.println("AUTO -> LEFT");
    }

    turning = true;

    turnStart = millis();

    return;
  }

  // --------------------------------------------------------------
  // Clear path
  // --------------------------------------------------------------

  moveForward();
}

// ================================================================
// ULTRASONIC DISTANCE
// ================================================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);

  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
    pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {

    return -1;
  }

  float distance =
    duration * 0.0343 / 2.0;

  return distance;
}

// ================================================================
// MOTOR A
// ================================================================

void motorA(int speedValue) {

  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0) {

    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    analogWrite(PWMA, speedValue);

  } else if (speedValue < 0) {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

    analogWrite(PWMA, -speedValue);

  } else {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

    analogWrite(PWMA, 0);
  }
}

// ================================================================
// MOTOR B
// ================================================================

void motorB(int speedValue) {

  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0) {

    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);

    analogWrite(PWMB, speedValue);

  } else if (speedValue < 0) {

    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);

    analogWrite(PWMB, -speedValue);

  } else {

    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);

    analogWrite(PWMB, 0);
  }
}

// ================================================================
// FORWARD
// ================================================================

void moveForward() {

  currentMovement = FORWARD;

  motorA(motorSpeed);
  motorB(motorSpeed);
}

// ================================================================
// BACKWARD
// ================================================================

void moveBackward() {

  currentMovement = BACKWARD;

  motorA(-motorSpeed);
  motorB(-motorSpeed);
}

// ================================================================
// LEFT
// ================================================================

void turnLeft() {

  currentMovement = LEFT;

  motorA(-motorSpeed);
  motorB(motorSpeed);
}

// ================================================================
// RIGHT
// ================================================================

void turnRight() {

  currentMovement = RIGHT;

  motorA(motorSpeed);
  motorB(-motorSpeed);
}

// ================================================================
// STOP
// ================================================================

void stopMotors() {

  currentMovement = STOPPED;

  motorA(0);
  motorB(0);
}

// ================================================================
// OLED EYES
// ================================================================

void updateEyes() {

  unsigned long now = millis();

  // --------------------------------------------------------------
  // Random/simple blinking
  // --------------------------------------------------------------

  if (!eyesClosed &&
      now - lastBlink > BLINK_INTERVAL) {

    eyesClosed = true;

    lastBlink = now;

    drawEyes();

    delay(100);

    eyesClosed = false;

    lastBlink = millis();

    drawEyes();

    return;
  }

  // --------------------------------------------------------------
  // Eye movement
  // --------------------------------------------------------------

  if (now - lastEyeUpdate >= EYE_UPDATE_TIME) {

    lastEyeUpdate = now;

    eyeOffset++;

    if (eyeOffset > 4) {

      eyeOffset = -4;
    }

    drawEyes();
  }
}

// ================================================================
// DRAW OLED EYES
// ================================================================

void drawEyes() {

  oled.clearBuffer();

  // --------------------------------------------------------------
  // Header
  // --------------------------------------------------------------

  oled.setFont(u8g2_font_5x7_tf);

  if (currentMode == AUTO_MODE) {

    oled.drawStr(2, 7, "AUTO");

  } else {

    oled.drawStr(2, 7, "MANUAL");
  }

  // --------------------------------------------------------------
  // Closed eyes
  // --------------------------------------------------------------

  if (eyesClosed) {

    oled.drawLine(18, 32, 45, 32);
    oled.drawLine(83, 32, 110, 32);

    oled.sendBuffer();

    return;
  }

  // --------------------------------------------------------------
  // Eye positions
  // --------------------------------------------------------------

  int leftX  = 17 + eyeOffset;
  int rightX = 82 + eyeOffset;

  // --------------------------------------------------------------
  // Eye shapes
  // --------------------------------------------------------------

  oled.drawRBox(
    leftX,
    19,
    29,
    29,
    7
  );

  oled.drawRBox(
    rightX,
    19,
    29,
    29,
    7
  );

  // --------------------------------------------------------------
  // Pupils
  // --------------------------------------------------------------

  oled.setDrawColor(0);

  oled.drawDisc(
    leftX + 15,
    33,
    6
  );

  oled.drawDisc(
    rightX + 15,
    33,
    6
  );

  oled.setDrawColor(1);

  // --------------------------------------------------------------
  // Movement indicator
  // --------------------------------------------------------------

  oled.setFont(u8g2_font_5x7_tf);

  switch (currentMovement) {

    case FORWARD:
      oled.drawStr(53, 60, "F");
      break;

    case BACKWARD:
      oled.drawStr(53, 60, "B");
      break;

    case LEFT:
      oled.drawStr(53, 60, "L");
      break;

    case RIGHT:
      oled.drawStr(53, 60, "R");
      break;

    default:
      oled.drawStr(53, 60, "S");
      break;
  }

  oled.sendBuffer();
}