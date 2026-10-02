🔌 BUDDY V1 - Wiring

BUDDY V1 is built around an ESP32 DevKit V1.  
The ESP32 controls the motors, sensors, OLED, GSM module and audio system.

🔋 Power Supply

- 2 × 18650 Li-ion batteries → 7.4V
- Battery → ON/OFF switch
- Switch → Buck Converter
- Buck Converter output → 5V
- 5V → ESP32, sensors, OLED and audio amplifier
- 5V → AMS1117-3.3V
- AMS1117 output → 3.3V logic supply
- TB6612 motor power (VM) → 7.4V battery supply
- SIM900A → separate 4.0V–4.2V high-current supply
- All GNDs are connected together

---

🧠 ESP32 Pin Connections

```text
HC-SR04 TRIG       → GPIO 5
HC-SR04 ECHO       → GPIO 18

DHT11 DATA         → GPIO 13

MQ-2 AO            → GPIO 34

OLED SDA           → GPIO 21
OLED SCL           → GPIO 22

SIM900A TXD        → GPIO 16 (RX2)
SIM900A RXD        → GPIO 17 (TX2)

TB6612 AIN1        → GPIO 26
TB6612 AIN2        → GPIO 27
TB6612 PWMA        → GPIO 32
TB6612 BIN1        → GPIO 33
TB6612 BIN2        → GPIO 14

MAX98357A BCLK     → GPIO 19
MAX98357A LRC      → GPIO 22
MAX98357A DIN      → GPIO 21

STATUS LED         → GPIO 2

TB6612 VM         → 7.4V motor supply
TB6612 VCC        → 3.3V
TB6612 GND        → GND

AIN1              → ESP32 GPIO 26
AIN2              → ESP32 GPIO 27
PWMA              → ESP32 GPIO 32

BIN1              → ESP32 GPIO 33
BIN2              → ESP32 GPIO 14

A01 + A02         → Left Motor
B01 + B02         → Right Motor

HC-SR04 Ultrasonic
VCC               → 5V
GND               → GND
TRIG              → GPIO 5
ECHO              → GPIO 18

DHT11
VCC               → 3.3V
DATA              → GPIO 13
GND               → GND

MQ-2 Gas Sensor
VCC               → 5V
GND               → GND
AO                → GPIO 34

OLED SSD1306
VCC               → 5V
GND               → GND
SDA               → GPIO 21
SCL               → GPIO 22

SIM900A GSM
SIM900A TXD       → ESP32 GPIO 16 (RX2)
SIM900A RXD       → ESP32 GPIO 17 (TX2)
SIM900A GND       → Common GND
SIM900A VCC       → 4.0V–4.2V dedicated supply

MAX98357A Audio
VIN               → 5V
GND               → GND
BCLK              → GPIO 19
LRC               → GPIO 22
DIN               → GPIO 21

SPK+              → Speaker +
SPK-              → Speaker -
