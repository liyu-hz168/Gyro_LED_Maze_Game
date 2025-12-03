#include <Adafruit_Protomatter.h>
#include <ArduinoBLE.h>
#include "mazes.h"

//#if defined(ESP32)
  // 'Safe' pins, not overlapping any peripherals:
  // GPIO.out: 4, 12, 13, 14, 15, 21, 27, GPIO.out1: 32, 33
  // Peripheral-overlapping pins, sorted from 'most expendible':
  // 16, 17 (RX, TX)
  // 25, 26 (A0, A1)
  // 18, 5, 9 (MOSI, SCK, MISO)
  // 22, 23 (SCL, SDA)
  uint8_t rgbPins[]  = {4, 12, 13, 14, 15, 21};
  uint8_t addrPins[] = {16, 17, 25, 26};
  uint8_t clockPin   = 27; // Must be on same port as rgbPins
  uint8_t latchPin   = 32;
  uint8_t oePin      = 33;
  enum COLOR {BLANK, MAGENTA, GREEN, RED, ORANGE, YELLOW, GOLD};
  uint8_t colors[][] = {{0,0,0},{255,0,255},{0,255,0},{255,0,0},{255,165,0},{255,255,0}, {218, 175, 55}};
  //MAGENTA for standard wall
  //GREEN for player
  //RED for walls that kill you
  //ORANGE for walls that turn on and off
  //YELLOW for end zone
  //GOLD for item (i.e. key)

//#endif

// ESP32 will recieve sensor reading from arduino nano sense 
//Scan for audino connection 
const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";


float8_t startX = 2.0;
float8_t startY = 2.0; 
float8_t curX = startX;
float8_t curY = startY;
unsigned long oldTime = 0;
uint8_t (*maze)[32];
uint8_t startingLv = 0; 
uint8_t currentLv = startingLv;


Adafruit_Protomatter matrix(
  32,          // Width of matrix (or matrix chain) in pixels
  4,           // Bit depth, 1-6
  1, rgbPins,  // # of matrix chains, array of 6 RGB pins for each
  4, addrPins, // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin, // Other matrix control pins
  false);      // No double-buffering here (see "doublebuffer" example)

// SETUP - RUNS ONCE AT PROGRAM START --------------------------------------

void setup(void) {
  Serial.begin(9600);
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

  Serial.println("* ESP32 ready to scan for connection!");
  BLE.scanForUuid(arduinoServiceUuid);

  matrix.drawPixel(startX,startY,matrix.color565(colors[GREEN][0], colors[GREEN][1]), colors[GREEN][2]);

  // Draw the maze
  renderNewMaze(startingLv); // Copy data to matrix buffers
}

// LOOP - RUNS REPEATEDLY AFTER SETUP --------------------------------------

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

          float8_t tiltX = data[0]/100.0;
          float8_t tiltY = data[1]/100.0;

          // TODO: Move ball
          // Erase old pixel
          matrix.drawPixel((uint8_t)curX, (uint8_t)curY, matrix.color565(0,0,0));

          //Maximum speed of 1 block every 200 milliseconds
          //Draw new 
          
          if (millis() - oldTime >= 200) {
            curX += tiltX;
            curY += tiltY; 
            oldTime = millis();
          }


          //Check for collisions
          switch(maze[(uint8_t)curY][(uint8_t)curX]) {
            //Blank square -- all good
            case 0:
              break;


            //Standard Wall - can't pass
            case 1:
              //Try moving along one axis and ignoring the other
              tiltX > tiltY ? curY -= tiltY: curX -= tiltX;

              //If that doesn't work, try moving only along the other axis instead
              if (maze[(uint8_t)curY][(uint8_t)curX] != BLANK && maze[(uint8_t)curY][(uint8_t)curX] != YELLOW
                   && maze[(uint8_t)curY][(uint8_t)curX] != GOLD) {


                    tiltX > tiltY ? (curY += tiltY, curX -= tiltX) : (curX += tiltX, curY -= tiltY);

                    //If this still fails, then don't move at all
                    if (maze[(uint8_t)curY][(uint8_t)curX] != BLANK && maze[(uint8_t)curY][(uint8_t)curX] != YELLOW
                      && maze[(uint8_t)curY][(uint8_t)curX] != GOLD) {
                        tiltX > tiltY ? curY -= tiltY : curX -= tiltX;
                      }
              }


              break; 

            //Player spawn point -- irrelevant case
            case 2:
              break;

            //Red wall -- death on contact
            case 3:

            //Orange wall -- can switch between on and off
            case 4:

            //Yellow wall -- win condition
            case 5:
            //maze = getMaze(winImageNum)

            //Gold color (collectible item)
            case 6:
            default:
              break;
          }
          curX = constrain(curX, 0.0, 31.0);
          curY = constrain(curY, 0.0, 31.0);
          matrix.drawPixel((uint8_t)curX, (uint8_t)curY, matrix.color565(colors[GREEN][0],colors[GREEN][1],colors[GREEN][2]));
          matrix.show();

          Serial.print("Tilt X: "); Serial.print(tiltX);
          Serial.print(" Y: "); Serial.println(tiltY);


        }
  }
  Serial.println("- Arduino disconnected.");
  BLE.scanForUuid(arduinoServiceUuid);
}

void renderNewMaze(uint8_t level){

  if(level < 5){
    maze = getMaze(level); 
    for(int x = 0; x < matrix.width(); x++){
      for(int y = 0; y < matrix.height(); y++){
          uint8_t curPixelColor = maze[y][x];
          matrix.drawPixel(x,y, matrix.color565(0,0,0));
          matrix.drawPixel(x, y, matrix.color565(colors[curPixelColor][0], colors[curPixelColor][1], colors[curPixelColor][2]));
      }
    }
    matrix.show(); // Copy data to matrix buffers
  }
  
}

void winState() {

}

void loseState() {

}
