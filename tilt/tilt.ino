#include <ArduinoBLE.h>
#include "Arduino_BMI270_BMM150.h"
// Set up BLE connection from Arduino Nano Sense to ESP32
// Send accelerometer and gyroscope data
// Custom uuid, randomly generated via https://www.uuidgenerator.net/
// Defining for Aduino Nano 33 BLE Sense
const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";
BLEService tiltService(arduinoServiceUuid); 
BLECharacteristic tiltCharacteristic(arduinoServiceCharacteristicUuid, BLERead | BLENotify, 2);


void setup() {

  //pinMode(LED_BUILTIN, OUTPUT); 

  Serial.begin(9600);
  //while(!Serial);

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
  BLE.addService(tiltService);
  uint8_t init[2] = {0, 0};
  tiltCharacteristic.writeValue(init, 2);
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

      if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(x, y, z);

        //FIXME 
        //Adjust scaling factor 12.9 as needed 
        int8_t tiltX = constrain((int)(x * 12.9), -127, 127);
        int8_t tiltY = constrain((int)(y * 12.9), -127, 127);

        uint8_t data[2] = {(uint8_t)tiltX, (uint8_t)tiltY};
        tiltCharacteristic.writeValue(data, 2);

        Serial.print("X: "); Serial.print(tiltX);
        Serial.print("  Y: "); Serial.println(tiltY);
      }
      
      delay(50);
    }

    Serial.println("* ESP32 disconnected.");
    
  }

}

