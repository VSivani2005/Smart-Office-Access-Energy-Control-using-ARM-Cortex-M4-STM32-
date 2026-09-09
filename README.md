# Smart Office Access and Energy Control System

## Overview

The **Smart Office Access and Energy Control System** is an embedded automation project developed using an **ARM Cortex-M4 STM32 microcontroller**.

The system combines **RFID-based access control**, **occupancy monitoring**, and **environmental sensing** to improve office security and automate energy consumption. Authorized users can access the office using RFID cards, while sensors monitor room occupancy, temperature, and ambient light to automatically control the door, fan, and lighting.

## Features

- 🔐 RFID-based secure access control
- 🚪 Automatic servo-controlled door unlocking and relocking
- 🚨 Unauthorized RFID card detection
- 🔒 Security lockout after 3 failed access attempts
- ⏱️ 30-second lockout duration
- 👤 PIR-based room occupancy monitoring
- 🌡️ DHT22 temperature and humidity monitoring
- 💨 Automatic temperature-based fan control
- 💡 LDR-based ambient light monitoring
- 🔆 Automatic lighting control using PWM
- ⚡ Automatic power saving when the room becomes empty
- 🔔 Buzzer-based access and security alerts
- 📊 Serial Monitor system status and sensor monitoring

---

## Hardware Components

| Component | Purpose |
|---|---|
| STM32 ARM Cortex-M4 | Main microcontroller |
| RC522 RFID Reader | User authentication and access control |
| RFID Cards | Authorized user identification |
| Servo Motor | Automatic door locking and unlocking |
| PIR Motion Sensor | Room occupancy detection |
| DHT22 Sensor | Temperature and humidity monitoring |
| LDR | Ambient light sensing |
| Buzzer | Access and security alerts |
| Fan | Temperature-based cooling |
| Light | Automatic lighting control |

---

## System Architecture

```text
                    +----------------------+
                    |   STM32 Cortex-M4    |
                    +----------+-----------+
                               |
        +----------------------+----------------------+
        |                      |                      |
        v                      v                      v
   RFID RC522              PIR Sensor             DHT22
        |                      |                      |
        v                      v                      v
 Access Control         Room Occupancy       Temperature/Humidity
        |                                             |
        v                                             v
 Servo Door Lock                                Fan Control
                                                      
                         +----------------+
                         |      LDR       |
                         +-------+--------+
                                 |
                                 v
                           Light Control
```

---

## Working Principle

### 1. RFID Access Control

The RC522 RFID reader scans RFID cards presented by users.

- If the card UID matches an authorized UID, access is granted.
- The door servo unlocks automatically.
- A confirmation beep is generated.
- The room is marked as occupied.
- The door automatically relocks after the configured unlock duration.

If an unauthorized RFID card is detected:

- The system records the failed access attempt.
- A warning buzzer alert is generated.
- After 3 failed attempts, the security lockout mechanism is activated.
- During the lockout period, further RFID access attempts are ignored.

---

### 2. Door Control

A servo motor is used as the electronic door locking mechanism.

- **Locked position:** 0°
- **Unlocked position:** 90°
- The door remains unlocked for approximately **7 seconds**.
- The system automatically relocks the door after the unlock duration expires.

---

### 3. Occupancy Detection

A PIR sensor monitors motion inside the office.

When the room is occupied:

- Motion updates the occupancy timer.
- Environmental controls remain active.

If no motion is detected for the configured occupancy timeout:

- The room is considered empty.
- The fan is turned OFF.
- The light is turned OFF.
- The system enters an energy-saving state.

---

### 4. Temperature-Based Fan Control

The DHT22 sensor continuously measures room temperature.

The fan speed is controlled using PWM:

| Temperature | Fan Operation |
|---|---|
| Below 25°C | OFF |
| 25°C–35°C | Variable PWM speed |
| Above or equal to 35°C | Maximum speed |

The fan operates only when the room is occupied.

---

### 5. Ambient Light Control

The LDR measures the surrounding light intensity.

The room lighting is automatically adjusted using PWM:

- Dark environment → Higher light brightness
- Bright environment → Reduced or OFF lighting
- Intermediate lighting conditions → Variable brightness

Lighting is also automatically turned OFF when the room becomes empty.

---

## Pin Configuration

| Component | STM32 Pin |
|---|---|
| RC522 SS | D10 |
| RC522 RST | D9 |
| Servo Motor | D8 |
| PIR Sensor | D7 |
| DHT22 | A1 |
| LDR | A0 |
| Buzzer | D4 |
| Fan PWM | D3 |
| Light PWM | D6 |

> Pin assignments may need to be adjusted depending on the STM32 board being used.

---

## System Configuration

```cpp
#define UNLOCK_DURATION_MS        7000
#define LOCKOUT_DURATION_MS       30000
#define MAX_FAIL_ATTEMPTS         3
#define FAIL_WINDOW_MS            120000
#define OCCUPANCY_TIMEOUT_MS      45000
#define ENV_READ_INTERVAL_MS      3000

#define TEMP_LOW                  25.0
#define TEMP_MAX                  35.0

#define FAN_MIN_RUNNING_PWM       120

#define LDR_DARK_VALUE            0
#define LDR_BRIGHT_VALUE          900

#define MIN_LIGHT_WHEN_OCCUPIED   40
```

---

## Required Libraries

Install the following libraries through the Arduino IDE Library Manager:

- `SPI`
- `MFRC522`
- `Servo`
- `DHT sensor library`

### Arduino IDE Installation

1. Open **Arduino IDE**.
2. Navigate to **Sketch → Include Library → Manage Libraries**.
3. Search for and install:
   - **MFRC522**
   - **DHT sensor library**
   - **Adafruit Unified Sensor**
4. Ensure the appropriate **STM32 board package** is installed.

---

## Authorized RFID Cards

Authorized RFID card UIDs are stored in the following array:

```cpp
const byte AUTHORIZED_UIDS[NUM_AUTHORIZED][4] =
{
  {0x13, 0x96, 0x22, 0x27},
  {0x3B, 0x63, 0x6A, 0x05}
};
```

Replace these UIDs with the RFID card UIDs used for your system.

---

## Serial Monitor Output

The system provides real-time information through the Serial Monitor.

Example output:

```text
[SYSTEM] SMART OFFICE SYSTEM STARTING

[SYSTEM] RFID initialized
[SYSTEM] DHT22 initialized
[SYSTEM] PIR initialized
[SYSTEM] Door locked

[RFID] UID: XX XX XX XX
[ACCESS] Authorized access
[DOOR] Door unlocked

[ENV] Temperature: 28.5 C
[ENV] Humidity: 65.0 %
[ENV] LDR Value: 450
[ENV] Fan PWM: 180
[ENV] Light PWM: 120
```

---

## Project Workflow

```text
START
  |
  v
Initialize STM32 and Peripherals
  |
  v
Initialize RFID, PIR, DHT22, LDR and Servo
  |
  v
Check Security Lockout
  |
  +---- Lockout Active ----> Ignore RFID Access
  |
  v
Detect PIR Motion
  |
  v
Scan RFID Card
  |
  +---- Authorized ----> Unlock Door
  |                         |
  |                         v
  |                    Mark Room Occupied
  |
  +---- Unauthorized --> Record Failed Attempt
                            |
                            v
                      3 Failed Attempts?
                            |
                    +-------+-------+
                    |               |
                   Yes              No
                    |               |
                    v               v
               Activate Lockout   Continue
  |
  v
Read Temperature and Light
  |
  v
Control Fan and Lighting
  |
  v
Check Occupancy Timeout
  |
  +---- Room Empty ----> Turn OFF Fan and Light
  |
  v
Repeat
```

---

## Technologies Used

- Embedded C / Arduino Framework
- STM32 ARM Cortex-M4
- GPIO
- ADC
- PWM
- UART
- SPI

---

## Future Improvements

Possible enhancements include:

- Cloud-based IoT monitoring
- Mobile application integration
- Wi-Fi connectivity
- MQTT communication
- Real-time energy consumption monitoring
- Multiple-user access logging
- Database integration
- Face recognition-based access control
- Email or SMS security alerts
- Web-based smart office dashboard

---
## License

This project is developed for academic and educational purposes.
