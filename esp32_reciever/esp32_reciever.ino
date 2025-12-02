#include <Adafruit_Protomatter.h>
#include <ArduinoBLE.h>

uint8_t rgbPins[]  = {4, 12, 13, 14, 15, 21};
uint8_t addrPins[] = {16, 17, 25, 26};
uint8_t clockPin   = 27; 
uint8_t latchPin   = 32;
uint8_t oePin      = 33;

uint8_t startX = 0;
uint8_t startY = 0; 
uint8_t curX = startX;
uint8_t curY = startY; 

Adafruit_Protomatter matrix(
  32,          // Width of matrix (or matrix chain) in pixels
  4,           // Bit depth, 1-6
  1, rgbPins,  // # of matrix chains, array of 6 RGB pins for each
  4, addrPins, // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin, // Other matrix control pins
  false);      // No double-buffering here (see "doublebuffer" example)

//Tester file for esp 32 to recieve sensor reading from arduino 
//Scan for audino connection 

const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";

void setup() {
  Serial.begin(115200);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {
    // DO NOT CONTINUE if matrix setup encountered an error.
    for(;;);
  }

  if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth Low Energy module failed!");
    while(1);
  }

  BLE.setLocalName("ESP32"); 
  // BLE.advertise();

  Serial.println("* ESP32 ready to scan for connection!");
  BLE.scanForUuid(arduinoServiceUuid);

  // Draw player
  // Player color 
  matrix.drawPixel(startX, startY, matrix.color565(0, 255, 0));
  matrix.show();

}

void loop() {

  BLE.poll();
  BLEDevice arduino = BLE.available();
  if (!arduino) {
    BLE.poll();                         
    return;                              
  }

  Serial.println("* Arduino found!");
  // Serial.print("* Advertised service UUID: ");
  // Serial.println(arduino.advertisedServiceUuid());
  Serial.println(" ");

  // In case connect to wrong arduino 
  if (arduino.advertisedServiceUuid() != arduinoServiceUuid) {
    Serial.println("* Wrong Arduino, keep scanning.");
    Serial.println();
    return;                           
  }

  BLE.stopScan();
  readArduino(arduino); 

  delay(100); 
}

void readArduino(BLEDevice arduino){
  if (arduino.connect()) {
      Serial.println("* Connected to arduino!");
      Serial.println(" ");
  } 
  else {
    Serial.println("* Connection to arduino failed!");
    Serial.println(" ");
    return;
  }

  Serial.println("- Discovering arduino tilt detection attributes...");
  if (arduino.discoverAttributes()) {
    Serial.println("* Attributes discovered!");
    Serial.println(" ");
  } 
  else {
    Serial.println("* Arduino tilt detection attributes discovery failed!");
    Serial.println(" ");
    arduino.disconnect();
    return;
  }

  BLECharacteristic tiltChar = arduino.characteristic(arduinoServiceCharacteristicUuid);
  if (!tiltChar) {
    Serial.println("* Tilt characteristic not found!");
    arduino.disconnect();
    return;
  }

  tiltChar.subscribe();

  while(arduino.connected()){
    BLE.poll();
    if (tiltChar.valueUpdated()) {
          uint8_t data[2];
          tiltChar.readValue(data, 2);

          int8_t tiltX = (int8_t)data[0];
          int8_t tiltY = (int8_t)data[1];

          // TODO: Move ball
          // Erase old pixel
          matrix.drawPixel(curX, curY, 0);
          //Draw new 
          curX = constrain(curX, 0, 31);
          curY = constrain(curY, 0, 31);
          curX += tiltX;
          curY += tiltY; 
          matrix.drawPixel(curX, curY, matrix.color565(0, 255, 0));
          matrix.show();

          Serial.print("Tilt X: "); Serial.print(tiltX);
          Serial.print(" Y: "); Serial.println(tiltY);


        }
  }
  Serial.println("- Arduino disconnected.");
  BLE.scanForUuid(arduinoServiceUuid);
}
