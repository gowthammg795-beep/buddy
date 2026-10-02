/*
  ============================================================
                    BUDDY V1 - AI ROBOT
  ============================================================

  Controller : ESP32 DevKit V1

  Features:
  - WiFi Access Point
  - Web dashboard
  - Manual motor control
  - Autonomous obstacle avoidance
  - HC-SR04 ultrasonic sensor
  - DHT11 temperature + humidity
  - MQ-2 gas sensor
  - SSD1306 OLED
  - SIM900A GSM UART
  - MAX98357A I2S speaker
  - Status LED

  ============================================================
                         PIN MAP
  ============================================================

  HC-SR04
    TRIG  -> GPIO 5
    ECHO  -> GPIO 18

  DHT11
    DATA  -> GPIO 13

  MQ-2
    AO    -> GPIO 34

  OLED SSD1306
    SDA   -> GPIO 21
    SCL   -> GPIO 22

  SIM900A
    TXD   -> GPIO 16 (ESP32 RX2)
    RXD   -> GPIO 17 (ESP32 TX2)

  TB6612FNG
    AIN1  -> GPIO 26
    AIN2  -> GPIO 27
    PWMA  -> GPIO 32
    BIN1  -> GPIO 33
    BIN2  -> GPIO 14
    STBY  -> 3.3V

  MAX98357A
    BCLK  -> GPIO 19
    LRC   -> GPIO 22
    DIN   -> GPIO 21

  STATUS LED
    GPIO 2

  ============================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "driver/i2s.h"

// ============================================================
// WIFI
// ============================================================

const char* AP_SSID = "BUDDY-V1";
const char* AP_PASSWORD = "buddy1234";

WebServer server(80);

// ============================================================
// PIN DEFINITIONS
// ============================================================

// HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 18

// DHT11
#define DHT_PIN 13
#define DHT_TYPE DHT11

// MQ-2
#define MQ2_PIN 34

// OLED
#define OLED_SDA 21
#define OLED_SCL 22

// SIM900A
#define SIM900_RX 16
#define SIM900_TX 17

// TB6612
#define AIN1 26
#define AIN2 27
#define PWMA 32

#define BIN1 33
#define BIN2 14

// MAX98357A
#define I2S_BCLK 19
#define I2S_LRC 22
#define I2S_DIN 21

// Status LED
#define STATUS_LED 2

// ============================================================
// OBJECTS
// ============================================================

DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_SSD1306 display(
  128,
  64,
  &Wire,
  -1
);

HardwareSerial SIM900(2);

// ============================================================
// MOTOR PWM
// ============================================================

#define MOTOR_PWM_FREQ 20000
#define MOTOR_PWM_RESOLUTION 8

int motorSpeed = 180;

// ============================================================
// ROBOT STATES
// ============================================================

enum RobotMode
{
  MANUAL_MODE,
  AUTO_MODE
};

RobotMode robotMode = MANUAL_MODE;

String motorState = "STOPPED";

// ============================================================
// SENSOR VARIABLES
// ============================================================

float distanceCM = 0;

float temperatureC = 0;
float humidity = 0;

int gasValue = 0;

// ============================================================
// TIMERS
// ============================================================

unsigned long lastSensorRead = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastAutoRun = 0;

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long OLED_INTERVAL = 500;

// ============================================================
// MOTOR FUNCTIONS
// ============================================================

void setLeftMotor(int speedValue)
{
  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0)
  {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    ledcWrite(PWMA, speedValue);
  }
  else if (speedValue < 0)
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    ledcWrite(PWMA, -speedValue);
  }
  else
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    ledcWrite(PWMA, 0);
  }
}

void setRightMotor(int speedValue)
{
  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0)
  {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    ledcWrite(PWMA, speedValue);
  }
  else if (speedValue < 0)
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    ledcWrite(PWMA, -speedValue);
  }
  else
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }
}

// ============================================================
// MOTOR CONTROL
// ============================================================

void stopRobot()
{
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);

  ledcWrite(PWMA, 0);

  motorState = "STOPPED";
}

void moveForward()
{
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);

  ledcWrite(PWMA, motorSpeed);

  motorState = "FORWARD";
}

void moveBackward()
{
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);

  ledcWrite(PWMA, motorSpeed);

  motorState = "BACKWARD";
}

void turnLeft()
{
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);

  ledcWrite(PWMA, motorSpeed);

  motorState = "LEFT";
}

void turnRight()
{
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);

  ledcWrite(PWMA, motorSpeed);

  motorState = "RIGHT";
}

// ============================================================
// ULTRASONIC
// ============================================================

float readDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
  {
    return 999;
  }

  float distance = duration * 0.0343 / 2.0;

  return distance;
}

// ============================================================
// SENSOR READING
// ============================================================

void readSensors()
{
  distanceCM = readDistance();

  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (!isnan(newTemperature))
  {
    temperatureC = newTemperature;
  }

  if (!isnan(newHumidity))
  {
    humidity = newHumidity;
  }

  gasValue = analogRead(MQ2_PIN);
}

// ============================================================
// OLED
// ============================================================

void updateOLED()
{
  /*
    OLED and MAX98357A share GPIO21/GPIO22.

    OLED is restored here after audio playback.
  */

  Wire.begin(OLED_SDA, OLED_SCL);

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("BUDDY V1");

  display.setCursor(0, 11);

  if (robotMode == AUTO_MODE)
  {
    display.println("MODE: AUTO");
  }
  else
  {
    display.println("MODE: MANUAL");
  }

  display.setCursor(0, 22);
  display.print("DIST: ");
  display.print(distanceCM, 1);
  display.println(" cm");

  display.setCursor(0, 33);
  display.print("TEMP: ");
  display.print(temperatureC, 1);
  display.println(" C");

  display.setCursor(0, 44);
  display.print("HUM: ");
  display.print(humidity, 0);
  display.println("%");

  display.setCursor(0, 55);
  display.print(motorState);

  display.display();
}

// ============================================================
// I2S AUDIO
// ============================================================

bool i2sReady = false;

void setupI2SAudio()
{
  /*
    GPIO21 and GPIO22 are shared with OLED.
    Audio is therefore initialized only when needed.
  */

  i2s_config_t i2s_config;

  memset(&i2s_config, 0, sizeof(i2s_config));

  i2s_config.mode =
      (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_TX
      );

  i2s_config.sample_rate = 16000;

  i2s_config.bits_per_sample =
      I2S_BITS_PER_SAMPLE_16BIT;

  i2s_config.channel_format =
      I2S_CHANNEL_FMT_RIGHT_LEFT;

  i2s_config.communication_format =
      I2S_COMM_FORMAT_I2S_MSB;

  i2s_config.intr_alloc_flags = 0;

  i2s_config.dma_buf_count = 8;

  i2s_config.dma_buf_len = 256;

  i2s_config.use_apll = false;

  i2s_config.tx_desc_auto_clear = true;

  i2s_config.fixed_mclk = 0;

  i2s_driver_install(
    I2S_NUM_0,
    &i2s_config,
    0,
    NULL
  );

  i2s_pin_config_t pin_config;

  pin_config.bck_io_num = I2S_BCLK;
  pin_config.ws_io_num = I2S_LRC;
  pin_config.data_out_num = I2S_DIN;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_set_pin(
    I2S_NUM_0,
    &pin_config
  );

  i2sReady = true;
}

// ============================================================
// SIMPLE BEEP
// ============================================================

void playTone(
  int frequency,
  int duration
)
{
  if (!i2sReady)
  {
    setupI2SAudio();
  }

  const int sampleRate = 16000;

  int totalSamples =
      (sampleRate * duration) / 1000;

  const int bufferSamples = 256;

  int16_t buffer[bufferSamples * 2];

  float phase = 0;

  float phaseIncrement =
      2.0 * PI * frequency / sampleRate;

  for (
    int sample = 0;
    sample < totalSamples;
    sample += bufferSamples
  )
  {
    int samplesToWrite =
        min(
          bufferSamples,
          totalSamples - sample
        );

    for (
      int i = 0;
      i < samplesToWrite;
      i++
    )
    {
      int16_t value =
          (int16_t)(
            sin(phase) * 7000
          );

      phase += phaseIncrement;

      if (phase >= 2.0 * PI)
      {
        phase -= 2.0 * PI;
      }

      buffer[i * 2] = value;
      buffer[i * 2 + 1] = value;
    }

    size_t bytesWritten = 0;

    i2s_write(
      I2S_NUM_0,
      buffer,
      samplesToWrite * 2 * sizeof(int16_t),
      &bytesWritten,
      portMAX_DELAY
    );
  }

  i2s_zero_dma_buffer(I2S_NUM_0);

  // Return pins to OLED
  i2s_driver_uninstall(I2S_NUM_0);

  i2sReady = false;

  Wire.begin(OLED_SDA, OLED_SCL);
}

// ============================================================
// GSM
// ============================================================

void sendSMS(
  String number,
  String message
)
{
  SIM900.println("AT");
  delay(500);

  SIM900.println("AT+CMGF=1");
  delay(500);

  SIM900.print("AT+CMGS=\"");
  SIM900.print(number);
  SIM900.println("\"");

  delay(500);

  SIM900.print(message);

  SIM900.write(26);

  delay(5000);
}

// ============================================================
// AUTO MODE
// ============================================================

void runAutonomousMode()
{
  if (distanceCM > 25)
  {
    moveForward();
  }
  else
  {
    stopRobot();

    delay(150);

    moveBackward();

    delay(300);

    stopRobot();

    delay(150);

    turnRight();

    delay(500);

    stopRobot();
  }
}

// ============================================================
// WEB PAGE
// ============================================================

String htmlPage()
{
  String page = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>BUDDY V1</title>

<style>

body {
  background:#080d18;
  color:white;
  font-family:Arial;
  text-align:center;
  margin:0;
  padding:20px;
}

h1 {
  color:#00e5ff;
}

.card {
  background:#111827;
  padding:20px;
  margin:15px auto;
  border-radius:15px;
  max-width:500px;
}

button {
  width:100px;
  height:55px;
  margin:5px;
  border:none;
  border-radius:12px;
  font-size:18px;
  font-weight:bold;
}

.forward {
  background:#22c55e;
}

.back {
  background:#ef4444;
}

.left,
.right {
  background:#3b82f6;
}

.stop {
  background:#f59e0b;
}

.auto {
  background:#8b5cf6;
}

</style>

</head>

<body>

<h1>🤖 BUDDY V1</h1>

<div class="card">

<h2>Robot Control</h2>

<button
class="forward"
onclick="cmd('forward')">
↑
</button>

<br>

<button
class="left"
onclick="cmd('left')">
←
</button>

<button
class="stop"
onclick="cmd('stop')">
STOP
</button>

<button
class="right"
onclick="cmd('right')">
→
</button>

<br>

<button
class="back"
onclick="cmd('backward')">
↓
</button>

</div>

<div class="card">

<h2>Mode</h2>

<button
class="auto"
onclick="cmd('auto')">
AUTO
</button>

<button
class="stop"
onclick="cmd('manual')">
MANUAL
</button>

</div>

<div class="card">

<h2>Sensor Data</h2>

<p>Distance:
<span id="distance">--</span> cm</p>

<p>Temperature:
<span id="temperature">--</span> °C</p>

<p>Humidity:
<span id="humidity">--</span> %</p>

<p>MQ-2:
<span id="gas">--</span></p>

<p>Mode:
<span id="mode">--</span></p>

<p>Motor:
<span id="motor">--</span></p>

</div>

<div class="card">

<h2>Audio</h2>

<button
class="auto"
onclick="cmd('beep')">
🔊 BEEP
</button>

</div>

<script>

function cmd(command)
{
  fetch('/cmd?value=' + command);
}

function updateData()
{
  fetch('/data')
  .then(response => response.json())
  .then(data => {

    document.getElementById(
      'distance'
    ).innerText = data.distance;

    document.getElementById(
      'temperature'
    ).innerText = data.temperature;

    document.getElementById(
      'humidity'
    ).innerText = data.humidity;

    document.getElementById(
      'gas'
    ).innerText = data.gas;

    document.getElementById(
      'mode'
    ).innerText = data.mode;

    document.getElementById(
      'motor'
    ).innerText = data.motor;

  });
}

setInterval(updateData, 1000);

updateData();

</script>

</body>

</html>
)rawliteral";

  return page;
}

// ============================================================
// WEB COMMAND HANDLER
// ============================================================

void handleCommand()
{
  if (!server.hasArg("value"))
  {
    server.send(
      400,
      "text/plain",
      "Missing command"
    );

    return;
  }

  String command =
      server.arg("value");

  if (command == "forward")
  {
    robotMode = MANUAL_MODE;
    moveForward();
  }

  else if (command == "backward")
  {
    robotMode = MANUAL_MODE;
    moveBackward();
  }

  else if (command == "left")
  {
    robotMode = MANUAL_MODE;
    turnLeft();
  }

  else if (command == "right")
  {
    robotMode = MANUAL_MODE;
    turnRight();
  }

  else if (command == "stop")
  {
    robotMode = MANUAL_MODE;
    stopRobot();
  }

  else if (command == "auto")
  {
    robotMode = AUTO_MODE;
    stopRobot();
  }

  else if (command == "manual")
  {
    robotMode = MANUAL_MODE;
    stopRobot();
  }

  else if (command == "beep")
  {
    playTone(1000, 300);
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// WEB DATA
// ============================================================

void handleData()
{
  String mode;

  if (robotMode == AUTO_MODE)
  {
    mode = "AUTO";
  }
  else
  {
    mode = "MANUAL";
  }

  String json = "{";

  json += "\"distance\":";
  json += String(distanceCM, 1);

  json += ",";

  json += "\"temperature\":";
  json += String(temperatureC, 1);

  json += ",";

  json += "\"humidity\":";
  json += String(humidity, 1);

  json += ",";

  json += "\"gas\":";
  json += String(gasValue);

  json += ",";

  json += "\"mode\":\"";
  json += mode;
  json += "\"";

  json += ",";

  json += "\"motor\":\"";
  json += motorState;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// WEB SERVER
// ============================================================

void setupWebServer()
{
  server.on(
    "/",
    HTTP_GET,
    []()
    {
      server.send(
        200,
        "text/html",
        htmlPage()
      );
    }
  );

  server.on(
    "/cmd",
    HTTP_GET,
    handleCommand
  );

  server.on(
    "/data",
    HTTP_GET,
    handleData
  );

  server.begin();

  Serial.println("Web server started");
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("       BUDDY V1 ROBOT");
  Serial.println("==============================");

  // ----------------------------------------------------------
  // GPIO
  // ----------------------------------------------------------

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(STATUS_LED, LOW);

  // ----------------------------------------------------------
  // MOTOR PWM
  // ----------------------------------------------------------

  ledcAttach(
    PWMA,
    MOTOR_PWM_FREQ,
    MOTOR_PWM_RESOLUTION
  );

  stopRobot();

  // ----------------------------------------------------------
  // DHT
  // ----------------------------------------------------------

  dht.begin();

  // ----------------------------------------------------------
  // MQ2
  // ----------------------------------------------------------

  pinMode(MQ2_PIN, INPUT);

  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      ))
  {
    Serial.println("OLED not found!");
  }
  else
  {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(
      SSD1306_WHITE
    );

    display.setCursor(0, 0);

    display.println(
      "BUDDY V1"
    );

    display.println();

    display.println(
      "Starting..."
    );

    display.display();
  }

  // ----------------------------------------------------------
  // SIM900A
  // ----------------------------------------------------------

  SIM900.begin(
    9600,
    SERIAL_8N1,
    SIM900_RX,
    SIM900_TX
  );

  Serial.println(
    "SIM900A UART ready"
  );

  // ----------------------------------------------------------
  // WIFI ACCESS POINT
  // ----------------------------------------------------------

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );

  Serial.println();
  Serial.println(
    "WiFi AP started"
  );

  Serial.print(
    "SSID: "
  );

  Serial.println(
    AP_SSID
  );

  Serial.print(
    "IP Address: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  setupWebServer();

  // ----------------------------------------------------------
  // STARTUP LED
  // ----------------------------------------------------------

  digitalWrite(
    STATUS_LED,
    HIGH
  );

  delay(500);

  digitalWrite(
    STATUS_LED,
    LOW
  );

  // ----------------------------------------------------------
  // STARTUP BEEP
  // ----------------------------------------------------------

  playTone(
    1000,
    200
  );

  Serial.println();
  Serial.println(
    "BUDDY V1 READY!"
  );

  Serial.print(
    "Open: http://"
  );

  Serial.print(
    WiFi.softAPIP()
  );

  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  server.handleClient();

  // ----------------------------------------------------------
  // SENSOR READING
  // ----------------------------------------------------------

  if (
    millis() - lastSensorRead
    >= SENSOR_INTERVAL
  )
  {
    lastSensorRead =
        millis();

    readSensors();

    Serial.println();
    Serial.println(
      "----- SENSOR DATA -----"
    );

    Serial.print(
      "Distance: "
    );

    Serial.print(
      distanceCM
    );

    Serial.println(
      " cm"
    );

    Serial.print(
      "Temperature: "
    );

    Serial.print(
      temperatureC
    );

    Serial.println(
      " C"
    );

    Serial.print(
      "Humidity: "
    );

    Serial.print(
      humidity
    );

    Serial.println(
      " %"
    );

    Serial.print(
      "MQ2: "
    );

    Serial.println(
      gasValue
    );
  }

  // ----------------------------------------------------------
  // AUTO MODE
  // ----------------------------------------------------------

  if (
    robotMode == AUTO_MODE
  )
  {
    if (
      millis() - lastAutoRun
      >= 100
    )
    {
      lastAutoRun =
          millis();

      if (distanceCM < 25)
      {
        stopRobot();

        digitalWrite(
          STATUS_LED,
          HIGH
        );

        delay(100);

        turnRight();

        delay(300);

        stopRobot();

        digitalWrite(
          STATUS_LED,
          LOW
        );
      }
      else
      {
        moveForward();
      }
    }
  }

  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  if (
    millis() - lastOLEDUpdate
    >= OLED_INTERVAL
  )
  {
    lastOLEDUpdate =
        millis();

    updateOLED();
  }
}
