#include <ArduinoBLE.h>
#include "Arduino_BMI270_BMM150.h"
// Set up BLE connection from Arduino Nano Sense to ESP32
// Send accelerometer and gyroscope data
// Custom uuid, randomly generated via https://www.uuidgenerator.net/
// Defining for Aduino Nano 33 BLE Sense
const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";
const char* gameCharacteristicUuid = "b824d997-58fc-4e70-9409-139acf0bed38"; // ESP32 -> Nano (Write)

BLEService tiltService(arduinoServiceUuid); 
// BLECharacteristic tiltCharacteristic(arduinoServiceCharacteristicUuid, BLERead | BLENotify, 2);
// Tilt: Notify + Read, length 2 bytes
BLECharacteristic tiltCharacteristic(
  arduinoServiceCharacteristicUuid,
  BLERead | BLENotify,
  2
);

// Game state: Write (and optionally Read), length 2 bytes
BLECharacteristic gameCharacteristic(
  gameCharacteristicUuid,
  BLERead | BLEWrite | BLEWriteWithoutResponse,
  2
);

#define RESET_BUTTON_PIN 9

void setup() {

  Serial.begin(9600);
  
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  if(!IMU.begin()){
    Serial.println("* IMU initialization failed!");
    while (1);
  }

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
}

void loop() {
  BLE.poll();
  // Wait for ESP32 to connect
  BLEDevice esp32 = BLE.central();
  Serial.println("- Discovering ESP32 device...");
  
  if (esp32) {
    Serial.print("Connected to ESP32: ");
    Serial.println(esp32.address());

    while (esp32.connected()) {
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
          Serial.print(incoming[0]);
          Serial.print(", ");
          Serial.println(incoming[1]);

          if (incoming[0] == 1 && incoming[1] == 1) {
              Serial.println("ESP32 sent win status!");
              // Play WIN.WAV
          }
          else if (incoming[0] == 2 && incoming[1] == 2) {
              Serial.println("ESP32 sent lose status!");
              // Play LOSS.WAV
          }
      }

      if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(x, y, z);

        float_t tiltX;
        float_t tiltY;

        //Deadzone of a tilt 0.2 or less
        if (x < 0.2 && x > -0.2)
          tiltX = 0.0;
        else if (x > 0.9)
          tiltX = 0.9;
        else if (x < -0.9)
          tiltX = -0.9;
        else
          tiltX = x;

        if (y < 0.2 && y > -0.2)
          tiltY = 0.0;
        else if (y > 0.9)
          tiltY = 0.9;
        else if (y < -0.9)
          tiltY = -0.9;
        else
          tiltY = y;
        
        uint8_t data[2] = {(uint8_t)((int8_t)(100.0 * tiltX)), (uint8_t)((int8_t)(100.0 * tiltY))};
        tiltCharacteristic.writeValue(data, 2);

      }
      
      delay(50);
    }

    Serial.println("* ESP32 disconnected.");
    
  }

}

