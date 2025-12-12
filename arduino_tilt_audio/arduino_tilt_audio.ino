/*
  SD card read/write
  SPI pins for Arduino Nano 33 BLE Sense:
  SD card attached to SPI bus as follows:
  ** SDO - pin 11
  ** SDI - pin 12
  ** CLK - pin 13
  ** CS - pin 10
*/

/*
  I2C pins for Arduino Nano 33 BLE Sense:
  Connected to Adafruit MCP4725, DAC conversion
  ** SDA - pin A4
  ** SCL - pin A5
*/

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ArduinoBLE.h>
#include "Arduino_BMI270_BMM150.h"

// For resetting game to first level (0)
#define RESET_BUTTON_PIN 9

// Set up BLE connection from Arduino Nano Sense to ESP32
// Send accelerometer and gyroscope data
// Custom UUID, randomly generated via https://www.uuidgenerator.net/
// Defining for Arduino Nano 33 BLE Sense
const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";
const char* gameCharacteristicUuid = "b824d997-58fc-4e70-9409-139acf0bed38"; // ESP32 -> Nano (Write)

BLEService tiltService(arduinoServiceUuid);

// Tilt: Notify + Read, length 2 bytes
BLECharacteristic tiltCharacteristic(
  arduinoServiceCharacteristicUuid,
  BLERead | BLENotify,
  2
);

// Game state: Write, length 2 bytes
BLECharacteristic gameCharacteristic(
  gameCharacteristicUuid,
  BLERead | BLEWrite | BLEWriteWithoutResponse,
  2
);

// Variable setup for audio output
Adafruit_MCP4725 dac;
#define DAC_RESOLUTION 8
#define CHIP_SELECT 10        // SD CS pin
#define SAMPLE_RATE 8000UL    // 8kHz for MCP4725 over I2C
#define BUFFER_SIZE 512

uint8_t buf[BUFFER_SIZE];
size_t bufLen = 0;
size_t bufPos = 0;
const unsigned long sampleInterval = 1000000UL / SAMPLE_RATE;
unsigned long lastSampleTime = 0;

enum SoundState { SOUND_IDLE, SOUND_PLAYING };
SoundState soundState = SOUND_IDLE;

File wavFile;
bool playingWin = false;

void setup() {
  Serial.begin(9600);

  // Initialize reset button
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // IMU for gyroscope reading
  if (!IMU.begin()) {
    Serial.println("* IMU initialization failed!");
    while (1);
  }

  // Bluetooth setup
  if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth Low Energy module failed!");
    while (1);
  }

  BLE.setLocalName("Nano 33 BLE tilt detection");
  BLE.setAdvertisedService(tiltService);
  tiltService.addCharacteristic(tiltCharacteristic);
  tiltService.addCharacteristic(gameCharacteristic);
  BLE.addService(tiltService);

  uint8_t init[2] = {0, 0};
  tiltCharacteristic.writeValue(init, 2);
  gameCharacteristic.writeValue(init, 2);
  BLE.advertise();

  Serial.println("* Arduino Nano 33 BLE Sense tilt detection service ready!");

  // SD card setup
  Serial.print("Initializing SD card...");
  if (!SD.begin(CHIP_SELECT)) {
    Serial.println("SD initialization failed!");
    while (1);
  }
  Serial.println("SD initialized.");

  // I2C wire initialization for DAC
  Wire.begin();
  Wire.setClock(400000);
  if (!dac.begin(0x62)) { // default MCP4725 address is 0x62
    Serial.println("MCP4725 not found");
  }
  Serial.println("MCP4725 found!!");

  Serial.println("Setup complete!");
}

void loop() {

  updateAudio();

  if (soundState != SOUND_PLAYING) {
    BLE.poll();
  }

  // Wait for ESP32 to connect
  BLEDevice esp32 = BLE.central();
  if (esp32) {
    Serial.print("Connected to ESP32: ");
    Serial.println(esp32.address());

    while (esp32.connected()) {
      updateAudio();

      // If audio is playing, DO NOT process IMU or BLE writes
      if (soundState == SOUND_PLAYING) continue;

      BLE.poll();

      float x, y, z;

      // Check reset button (active LOW)
      if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        uint8_t resetData[2] = {255, 255};  // 255, 255 = reset value
        tiltCharacteristic.writeValue(resetData, 2);
        delay(300);
        continue;
      }

      // If receive game status from ESP32
      if (gameCharacteristic.written()) {
        uint8_t incoming[2];
        gameCharacteristic.readValue(incoming, 2);

        Serial.print("Received from ESP32: ");

        if (incoming[0] == 1 && incoming[1] == 1) {
          Serial.println("ESP32 sent win status!");
          startSound("WIN.WAV");
          continue;
        } else if (incoming[0] == 2 && incoming[1] == 2) {
          startSound("DEATH.WAV");
          continue;
        }
      }

      if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(x, y, z);

        float_t tiltX, tiltY;

        // Deadzone of a tilt 0.2 or less
        tiltX = (x < 0.2 && x > -0.2) ? 0.0 : (x > 0.9 ? 0.9 : (x < -0.9 ? -0.9 : x));
        tiltY = (y < 0.2 && y > -0.2) ? 0.0 : (y > 0.9 ? 0.9 : (y < -0.9 ? -0.9 : y));

        uint8_t data[2] = {
          (uint8_t)((int8_t)(100.0 * tiltX)),
          (uint8_t)((int8_t)(100.0 * tiltY))
        };
        tiltCharacteristic.writeValue(data, 2);
      }
    }

    Serial.println("* ESP32 disconnected.");
  }
}

inline uint16_t map8_to_12(uint8_t s) {
  return (uint16_t)s << 4;
}

void updateAudio() {
  if (soundState != SOUND_PLAYING) return;

  refillBuffer();

  unsigned long now = micros();
  if ((now - lastSampleTime) >= sampleInterval) {
    lastSampleTime += sampleInterval;

    if (bufPos < bufLen) {
      uint8_t sample = buf[bufPos++];
      dac.setVoltage(map8_to_12(sample), false);
    } else {
      // End of file → go back to BLE state
      stopSound();
    }
  }
}

void refillBuffer() {
  const size_t REFILL_THRESHOLD = 128; // start refilling when <128 bytes remain
  if (bufLen - bufPos > REFILL_THRESHOLD) return;
  if (!wavFile) return;

  // Move remaining data to front of buffer
  size_t remain = bufLen - bufPos;
  if (remain && bufPos) {
    memmove(buf, buf + bufPos, remain);
  }
  bufLen = remain;
  bufPos = 0;

  int toRead = (int)(BUFFER_SIZE - bufLen);
  int r = wavFile.read(buf + bufLen, toRead);
  if (r <= 0) {
    stopSound();
    return;
  }
  bufLen += (size_t)r;
}

void startSound(const char* filename) {
  if (soundState == SOUND_PLAYING) return;

  pauseBLE();

  wavFile = SD.open(filename);
  if (!wavFile) return;

  wavFile.seek(44); // skip header
  bufLen = bufPos = 0;
  refillBuffer();

  lastSampleTime = micros();
  soundState = SOUND_PLAYING;
}

void stopSound() {
  if (wavFile) wavFile.close();
  soundState = SOUND_IDLE;

  // Silence DAC
  dac.setVoltage(2048, false);
  resumeBLE();
}

void pauseBLE() {
  BLE.stopAdvertise();
  BLE.disconnect();
  BLE.end(); // fully shuts down SoftDevice
}

void resumeBLE() {
  BLE.begin();
  BLE.setLocalName("Nano 33 BLE tilt detection");
  BLE.setAdvertisedService(tiltService);
  BLE.addService(tiltService);
  BLE.advertise();
}
