/*
   ================================================================
   SMART OFFICE ACCESS AND ENERGY CONTROL SYSTEM
   ARM Cortex-M4 STM32

   Components:
   - STM32 Cortex-M4
   - RC522 RFID Reader
   - Servo Motor Door Lock
   - PIR Motion Sensor
   - DHT22 Temperature/Humidity Sensor
   - LDR Ambient Light Sensor
   - Buzzer
   - Fan PWM / Relay Control
   - Light PWM / Relay Control

   Functions:
   - RFID secure access
   - Automatic door unlock and relock
   - Unauthorized attempt detection
   - 3 failed attempts -> 30 second lockout
   - PIR occupancy monitoring
   - Temperature based fan control
   - Ambient light based lighting control
   - Automatic power OFF when room is empty
   ================================================================
*/

#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <DHT.h>


// ================================================================
// PIN CONFIGURATION
// ================================================================

// RFID RC522
#define RFID_SS_PIN       D10
#define RFID_RST_PIN      D9

// Servo Door Lock
#define SERVO_PIN         D8

// Buzzer
#define BUZZER_PIN        D4

// PIR Motion Sensor
#define PIR_PIN           D7

// DHT22
#define DHT_PIN           A1
#define DHT_TYPE          DHT22

// Fan Control
#define FAN_PWM_PIN       D3

// LDR
#define LDR_PIN           A0

// Room Light Control
#define LIGHT_PWM_PIN     D6


// ================================================================
// SYSTEM SETTINGS
// ================================================================

#define BUZZER_FREQ               2500

// Door unlock time
#define UNLOCK_DURATION_MS        7000

// Security lockout time
#define LOCKOUT_DURATION_MS       30000

// Failed card attempts allowed
#define MAX_FAIL_ATTEMPTS         3

// Time window for failed attempts
#define FAIL_WINDOW_MS            120000

// Occupancy timeout
#define OCCUPANCY_TIMEOUT_MS      45000

// Environmental sensor update
#define ENV_READ_INTERVAL_MS      3000


// ================================================================
// FAN SETTINGS
// ================================================================

#define TEMP_LOW                  25.0
#define TEMP_MAX                  35.0

#define FAN_MIN_RUNNING_PWM       120


// ================================================================
// LIGHT SETTINGS
// ================================================================

// Adjust these according to your LDR readings

#define LDR_DARK_VALUE            0
#define LDR_BRIGHT_VALUE          900

#define MIN_LIGHT_WHEN_OCCUPIED   40


// ================================================================
// AUTHORIZED RFID CARDS
// ================================================================

// Number of authorized cards

const byte NUM_AUTHORIZED = 2;


// Replace these UIDs with your own RFID card UIDs

const byte AUTHORIZED_UIDS[NUM_AUTHORIZED][4] =
{
  {0x13, 0x96, 0x22, 0x27},
  {0x3B, 0x63, 0x6A, 0x05}
};


// ================================================================
// OBJECTS
// ================================================================

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

Servo doorServo;

DHT dht(DHT_PIN, DHT_TYPE);


// ================================================================
// SYSTEM STATE VARIABLES
// ================================================================

bool doorUnlocked = false;

bool inLockout = false;

bool roomOccupied = false;

bool pirTriggered = false;


int failCount = 0;

int lastFanPWM = 0;

int lastLightPWM = 0;


unsigned long unlockStart = 0;

unsigned long lockoutStart = 0;

unsigned long firstFailTime = 0;

unsigned long lastMotionTime = 0;

unsigned long lastHeartbeat = 0;

unsigned long lastEnvRead = 0;


// ================================================================
// BUZZER FUNCTIONS
// ================================================================

void buzzerOn()
{
  tone(BUZZER_PIN, BUZZER_FREQ);
}


void buzzerOff()
{
  noTone(BUZZER_PIN);
}


void buzzShort(int count)
{
  for (int i = 0; i < count; i++)
  {
    tone(BUZZER_PIN, BUZZER_FREQ, 250);

    delay(300);
  }

  buzzerOff();
}


// ================================================================
// DOOR FUNCTIONS
// ================================================================

void unlockDoor()
{
  doorServo.write(90);

  doorUnlocked = true;

  unlockStart = millis();

  Serial.println("[DOOR] Door unlocked");
}


void lockDoor()
{
  doorServo.write(0);

  doorUnlocked = false;

  Serial.println("[DOOR] Door locked");
}


// ================================================================
// FAN CONTROL
// ================================================================

void setFan(int duty)
{
  lastFanPWM = constrain(duty, 0, 255);

  analogWrite(FAN_PWM_PIN, lastFanPWM);
}


int tempToFanPWM(float temperature)
{
  // Fan OFF below 25C

  if (temperature < TEMP_LOW)
  {
    return 0;
  }


  // Full speed above 35C

  if (temperature >= TEMP_MAX)
  {
    return 255;
  }


  // Variable fan speed between 25C and 35C

  return map(
    (int)(temperature * 10),
    (int)(TEMP_LOW * 10),
    (int)(TEMP_MAX * 10),
    FAN_MIN_RUNNING_PWM,
    255
  );
}


// ================================================================
// LIGHT CONTROL
// ================================================================

void setLight(int duty)
{
  lastLightPWM = constrain(duty, 0, 255);

  analogWrite(LIGHT_PWM_PIN, lastLightPWM);
}


int ldrToLightPWM(int ldrValue)
{
  int duty;


  // Dark room -> full brightness

  if (ldrValue <= LDR_DARK_VALUE)
  {
    duty = 255;
  }


  // Bright room -> light OFF

  else if (ldrValue >= LDR_BRIGHT_VALUE)
  {
    duty = 0;
  }


  // Intermediate brightness

  else
  {
    duty = map(
      ldrValue,
      LDR_DARK_VALUE,
      LDR_BRIGHT_VALUE,
      255,
      0
    );
  }


  // Minimum light while room occupied

  if (roomOccupied &&
      duty > 0 &&
      duty < MIN_LIGHT_WHEN_OCCUPIED)
  {
    duty = MIN_LIGHT_WHEN_OCCUPIED;
  }


  return constrain(duty, 0, 255);
}


// ================================================================
// ROOM OCCUPANCY
// ================================================================

void setOccupied()
{
  if (!roomOccupied)
  {
    Serial.println("[OCCUPANCY] Room occupied");
  }


  roomOccupied = true;

  pirTriggered = false;

  lastMotionTime = millis();
}


void setEmpty()
{
  roomOccupied = false;

  pirTriggered = false;


  // Energy saving

  setFan(0);

  setLight(0);


  Serial.println("[OCCUPANCY] Room empty");

  Serial.println("[ENERGY] Fan OFF");

  Serial.println("[ENERGY] Light OFF");
}


// ================================================================
// ENVIRONMENT MONITORING
// ================================================================

void updateEnvironment()
{
  float temperature = dht.readTemperature();

  float humidity = dht.readHumidity();

  int ldrValue = analogRead(LDR_PIN);


  // Check DHT sensor

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("[ENV] DHT22 read failed");

    return;
  }


  // Only operate appliances when occupied

  if (roomOccupied)
  {
    int fanPWM = tempToFanPWM(temperature);

    int lightPWM = ldrToLightPWM(ldrValue);


    setFan(fanPWM);

    setLight(lightPWM);
  }

  else
  {
    setFan(0);

    setLight(0);
  }


  // Display information

  Serial.println("------------------------------------");

  Serial.print("[ENV] Temperature: ");

  Serial.print(temperature, 1);

  Serial.println(" C");


  Serial.print("[ENV] Humidity: ");

  Serial.print(humidity, 1);

  Serial.println(" %");


  Serial.print("[ENV] LDR Value: ");

  Serial.println(ldrValue);


  Serial.print("[ENV] Fan PWM: ");

  Serial.println(lastFanPWM);


  Serial.print("[ENV] Light PWM: ");

  Serial.println(lastLightPWM);


  Serial.println("------------------------------------");
}


// ================================================================
// RFID UID PRINTING
// ================================================================

void printUID()
{
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
    {
      Serial.print("0");
    }

    Serial.print(rfid.uid.uidByte[i], HEX);


    if (i < rfid.uid.size - 1)
    {
      Serial.print(" ");
    }
  }
}


// ================================================================
// CHECK AUTHORIZED CARD
// ================================================================

bool isAuthorized()
{
  // Only 4-byte UID cards expected

  if (rfid.uid.size != 4)
  {
    return false;
  }


  for (byte card = 0; card < NUM_AUTHORIZED; card++)
  {
    bool match = true;


    for (byte b = 0; b < 4; b++)
    {
      if (rfid.uid.uidByte[b] != AUTHORIZED_UIDS[card][b])
      {
        match = false;

        break;
      }
    }


    if (match)
    {
      return true;
    }
  }


  return false;
}


// ================================================================
// FAILED ACCESS HANDLING
// ================================================================

void handleFailedAttempt()
{
  unsigned long now = millis();


  // Reset failure count after time window

  if (failCount > 0 &&
      now - firstFailTime > FAIL_WINDOW_MS)
  {
    failCount = 0;
  }


  if (failCount == 0)
  {
    firstFailTime = now;
  }


  failCount++;


  Serial.print("[SECURITY] Unauthorized card");

  Serial.print(" - Attempt ");

  Serial.print(failCount);

  Serial.print(" / ");

  Serial.println(MAX_FAIL_ATTEMPTS);


  // Lockout after maximum failed attempts

  if (failCount >= MAX_FAIL_ATTEMPTS)
  {
    Serial.println("[SECURITY] LOCKOUT ACTIVATED");

    inLockout = true;

    lockoutStart = millis();

    failCount = 0;


    buzzerOn();
  }

  else
  {
    // Warning buzzer

    buzzShort(3);
  }
}


// ================================================================
// RFID HANDLER
// ================================================================

void handleRFID()
{
  if (doorUnlocked)
  {
    return;
  }


  // Check for new card

  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }


  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }


  Serial.print("[RFID] UID: ");

  printUID();

  Serial.println();


  // Authorized access

  if (isAuthorized())
  {
    failCount = 0;


    Serial.println("[ACCESS] Authorized access");


    // Mark room occupied

    setOccupied();


    // Unlock door

    unlockDoor();


    // Access confirmation beep

    buzzShort(1);
  }


  // Unauthorized access

  else
  {
    handleFailedAttempt();
  }


  // Stop RFID communication

  rfid.PICC_HaltA();

  rfid.PCD_StopCrypto1();
}


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);


  delay(2000);


  Serial.println();

  Serial.println("====================================");

  Serial.println(" SMART OFFICE SYSTEM STARTING");

  Serial.println("====================================");


  // Pin modes

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(PIR_PIN, INPUT);

  pinMode(FAN_PWM_PIN, OUTPUT);

  pinMode(LIGHT_PWM_PIN, OUTPUT);


  // Initial states

  buzzerOff();

  setFan(0);

  setLight(0);


  // Start DHT22

  dht.begin();


  // Initialize door servo

  doorServo.attach(SERVO_PIN, 500, 2400);

  lockDoor();


  // Initialize SPI for STM32

  SPI.begin();


  // IMPORTANT:
  // Do NOT use SPI.setClockDivider()
  // STM32 SPI library does not support it


  // Initialize RC522

  rfid.PCD_Init();


  delay(500);


  Serial.println("[SYSTEM] RFID initialized");

  Serial.println("[SYSTEM] DHT22 initialized");

  Serial.println("[SYSTEM] PIR initialized");

  Serial.println("[SYSTEM] Door locked");

  Serial.println("[SYSTEM] Waiting for PIR stabilization...");


  // PIR stabilization

  delay(5000);


  Serial.println("[SYSTEM] READY");

  Serial.println("====================================");
}


// ================================================================
// MAIN LOOP
// ================================================================

void loop()
{
  unsigned long currentTime = millis();


  // --------------------------------------------------------------
  // SYSTEM HEARTBEAT
  // --------------------------------------------------------------

  if (currentTime - lastHeartbeat >= 2000)
  {
    Serial.print("[SYSTEM] Uptime: ");

    Serial.print(currentTime / 1000);

    Serial.print(" sec | Occupied: ");

    Serial.print(roomOccupied ? "YES" : "NO");


    Serial.print(" | Door: ");

    Serial.print(
      doorUnlocked ? "UNLOCKED" : "LOCKED"
    );


    Serial.print(" | Motion: ");

    Serial.println(
      digitalRead(PIR_PIN) ?
      "DETECTED" :
      "NONE"
    );


    lastHeartbeat = currentTime;
  }


  // --------------------------------------------------------------
  // AUTOMATIC DOOR RELOCK
  // --------------------------------------------------------------

  if (doorUnlocked &&
      currentTime - unlockStart >= UNLOCK_DURATION_MS)
  {
    lockDoor();
  }


  // --------------------------------------------------------------
  // SECURITY LOCKOUT
  // --------------------------------------------------------------

  if (inLockout)
  {
    buzzerOn();


    if (currentTime - lockoutStart >=
        LOCKOUT_DURATION_MS)
    {
      inLockout = false;

      buzzerOff();


      Serial.println(
        "[SECURITY] Lockout expired"
      );
    }


    // During lockout ignore access

    return;
  }


  // --------------------------------------------------------------
  // PIR MOTION DETECTION
  // --------------------------------------------------------------

  bool motion = digitalRead(PIR_PIN);


  // Motion in empty room

  if (motion &&
      !roomOccupied &&
      !pirTriggered)
  {
    pirTriggered = true;


    Serial.println(
      "[PIR] Motion detected in empty room"
    );


    Serial.println(
      "[SECURITY] Scan authorized RFID card"
    );


    // Short warning beep

    tone(
      BUZZER_PIN,
      BUZZER_FREQ,
      200
    );
  }


  // Motion while occupied

  if (motion &&
      roomOccupied)
  {
    lastMotionTime = currentTime;
  }


  // --------------------------------------------------------------
  // RFID ACCESS
  // --------------------------------------------------------------

  handleRFID();


  // --------------------------------------------------------------
  // OCCUPANCY TIMEOUT
  // --------------------------------------------------------------

  if (roomOccupied &&
      currentTime - lastMotionTime >=
      OCCUPANCY_TIMEOUT_MS)
  {
    setEmpty();
  }


  // --------------------------------------------------------------
  // ENVIRONMENT UPDATE
  // --------------------------------------------------------------

  if (currentTime - lastEnvRead >=
      ENV_READ_INTERVAL_MS)
  {
    updateEnvironment();

    lastEnvRead = currentTime;
  }
}

