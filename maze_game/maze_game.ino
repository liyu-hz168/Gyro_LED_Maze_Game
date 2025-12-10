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
  enum COLOR {BLANK, MAGENTA, GREEN, RED, BLUE, YELLOW, GOLD};
  uint8_t colors[7][3] = {{0,0,0},{255,0,255},{0,255,0},{255,0,0},{0,100,255},{255,255,0},{218, 175, 55}};
  //MAGENTA for standard wall
  //GREEN for player
  //RED for walls that kill you
  //BLUE for walls that turn on and off
  //YELLOW for end zone
  //GOLD for item (i.e. key)

//#endif

// ESP32 will recieve sensor reading from arduino nano sense 
//Scan for audino connection 
const char* arduinoServiceUuid = "d8b61347-0ce7-445e-830b-40c976921b35";
const char* arduinoServiceCharacteristicUuid = "843fe463-99f5-4acc-91a2-93b82e5b36b2";


float_t startX = 2.0;
float_t startY = 2.0; 
float_t curX = startX;
float_t curY = startY;
unsigned long oldTime = 0;
uint8_t (*maze)[32];
uint8_t currentLv = 0;


unsigned long lastFlashTime = 0;
bool blueWallsOn = true;


//coordinates of each gold key (which unlocks a red gate), maximum of 3 per level
uint8_t key_coords[5][3][2] = {
  {
    {21,10},{3,25},{-1,-1}
  },

  {
    {},{},{}
  },

  {
    {},{},{}
  },

  {

  },

  {

  }
};

//coordinates for the gates associated with each golden key, maximum of 3 per level. Point ONLY to the topmost or leftmost pixel of the gate
uint8_t gate_coords[5][3][2] = {
 {
  {7, 26}, {25,1}, {-1,-1}
 },
 {},
 {},
 {},
 {},
};


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

  matrix.drawPixel(startX,startY,matrix.color565(colors[GREEN][0], colors[GREEN][1], colors[GREEN][2]);

  // Draw the maze
  renderNewMaze(currentLv); // Copy data to matrix buffers
  oldTime = millis();
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

  nextlvl:
  while(arduino.connected()){
    BLE.poll();
    if (tiltChar.valueUpdated()) {
          uint8_t data[2];
          tiltChar.readValue(data, 2);

          float_t tiltX = (int8_t)data[0]/100.0;
          float_t tiltY = (int8_t)data[1]/100.0;

          // TODO: Move ball
          // Erase old pixel

          //Maximum speed of 1 block every 200 milliseconds
          //Draw new 
          
          if (millis() - oldTime >= 25) {
            matrix.drawPixel((uint8_t)curX, (uint8_t)curY, matrix.color565(0,0,0));
            curX += tiltX;
            curY += tiltY; 
            oldTime = millis();
          

            //Check to see if touching red   (Use for different color)
            // if (maze[(uint8_t)curY][(uint8_t)curX] == 3 || touchingRed(curX, curY)) {
            //   // red death animation
            //   matrix.drawPixel((uint8_t)curX, (uint8_t)curY, 
            //       matrix.color565((colors[GREEN][0]+colors[RED][0])/2,
            //                       (colors[GREEN][1]+colors[RED][1])/2,
            //                       (colors[GREEN][2]+colors[RED][2])/2));
            //   matrix.show();
            //   delay(1500);
            //   loseState();
            //   goto nextlvl;
            // }

            //Check for collisions
            switch(maze[(uint8_t)curY][(uint8_t)curX]) {
              //Blank square -- all good
              case 0:
                break;



//Standard Wall - can't pass
              case 1:
                collisionDetection(tiltX, tiltY);
                break;

              //Blue wall -- switches between on and off at regular intervals (treat as normal wall when on)
              case 4:
                if(blueWallsOn) {
                  //If you are on a blank square that turns blue WHILE you are on it, you die
                  if (maze[(uint8_t)(curY-tiltY)][(uint8_t)(curX-tiltX)] == BLUE) {
                    matrix.drawPixel((uint8_t)(curX-tiltX), (uint8_t)(curY-tiltY), 
                      matrix.color565((colors[GREEN][0]+colors[BLUE][0])/2,(colors[GREEN][1]+colors[BLUE][1])/2,(colors[GREEN][2]+colors[BLUE][2])/2)
                    );
                    delay(2000);
                    loseState();
                    goto nextlvl;
                  }

                  collisionDetection(tiltX, tiltY);

                }
                break;


              //Player spawn point -- irrelevant case
              case 2:
                break;

              //Red wall -- death on contact
              case 3:
                matrix.drawPixel((uint8_t)curX, (uint8_t)curY, 
                  matrix.color565((colors[GREEN][0]+colors[RED][0])/2,(colors[GREEN][1]+colors[RED][1])/2,(colors[GREEN][2]+colors[RED][2])/2)
                );
                matrix.show();
                delay(2000);
                loseState();
                goto nextlvl;

              //Yellow wall -- win condition
              case 5:
                matrix.drawPixel((uint8_t)curX, (uint8_t)curY, matrix.color565(colors[GREEN][0],colors[GREEN][1],colors[GREEN][2]));
                matrix.show();
                delay(1500);
                winState();
                goto nextlvl;

              //Gold color (collectible item)
              case 6:
                //Erase all gold pixels in radius 2
                int gateNum = -1;
                for(int yCount = (uint8_t)curY-2; yCount <= (uint8_t)curY+2; yCount++)
                  for(int xCount = (uint8_t)curX -2; xCount <= (uint8_t)curX+2; xCount++) {
                    if (xCount < 0 || xCount > 31 || yCount < 0 || yCount > 31)
                      continue;
                    if (maze[yCount][xCount] == GOLD) {
                      maze[yCount][xCount] = -6;
                      matrix.drawPixel(xCount, yCount, matrix.color565(0,0,0));

                      //match this key to a gate
                      for (int i = 0; i < 3 && gateNum < 0; i++) {
                        int coords[2] = key_coords[currentLv][i];
                        if (coords[0] == xCount && coords[1] == yCount) {
                          gateNum = i;
                          break;
                        }
                      }

                      removeGate(gateNum);
                    }
                }
                matrix.show();
                break;

              //any pixels that are not blank by default but can become blank
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
  }
  Serial.println("- Arduino disconnected.");
  BLE.scanForUuid(arduinoServiceUuid);
}

void renderNewMaze(uint8_t level){

  if(level < 11){
    maze = getMaze(level); 
    for(int x = 0; x < matrix.width(); x++){
      for(int y = 0; y < matrix.height(); y++){
          uint8_t curPixelColor = maze[y][x];
          curPixelColor < BLANK? curPixelColor = BLANK: curPixelColor = curPixelColor;
          matrix.drawPixel(x,y, matrix.color565(0,0,0));
          matrix.drawPixel(x, y, matrix.color565(colors[curPixelColor][0], colors[curPixelColor][1], colors[curPixelColor][2]));
      }
    }
    matrix.show(); // Copy data to matrix buffers
  }
  
}

void flashBlue(){
  unsigned long now = millis();
  if (now - lastFlashTime >= 2000) {
    lastFlashTime = millis(); 
    blueWallsOn = !blueWallsOn;

    for(int x = 0; x < matrix.width(); x++){
      for(int y = 0; y < matrix.height(); y++){
        uint8_t curPixelColor = maze[y][x];
        // If blue, flash the wall turn off
        if(curPixelColor == BLUE){
          if (blueWallsOn) {
            matrix.drawPixel(x, y, matrix.color565(colors[BLUE][0], colors[BLUE][1], colors[BLUE][2]));
            }
          else {
            matrix.drawPixel(x, y, matrix.color565(0,0,0));
          }
        }
      }
    }

    // Redraw player
    matrix.drawPixel((uint8_t)curX, (uint8_t)curY, matrix.color565(colors[GREEN][0], colors[GREEN][1], colors[GREEN][2]));
    matrix.show();
  }
     
}

bool touchingRed(float x, float y) {
  int cx = (int)x;
  int cy = (int)y;

  // Check current tile and 4 neighbors
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      // Don't check (-1, -1)(-1, 1)(1, -1)(1, 1)
      if ((dy == -1 && dx == -1) || (dy == -1 && dx == 1) || (dy == 1 && dx == -1) || (dy == 1 && dx == 1)){
        continue;
      }
      int nx = cx + dx;
      int ny = cy + dy;

      if (nx < 0 || nx > 31 || ny < 0 || ny > 31) continue;
      if (maze[ny][nx] == 3) { 
        return true;
      }
    }
  }
  return false;
}

void collisionDetection(float_t tiltX, float_t tiltY){
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
}

void refreshMaze() {
  for (int i = 0; i < 31; i++)
    for (int j = 0; j < 31; j++)
      if (maze[i][j] <= 0) {
        maze[i][j] *= -1;
      }
}

void winState() {
  renderNewMaze(10);
  currentLv++;
  delay(5000);
  oldTime = millis();
  refreshMaze();
  renderNewMaze(currentLv);
  curX = startX;
  curY = startY;
  matrix.drawPixel(startX,startY,matrix.color565(colors[GREEN][0], colors[GREEN][1]), colors[GREEN][2]);
  matrix.show();
}

void loseState() {
  renderNewMaze(9);
  delay(5000);
  oldTime = millis();
  refreshMaze();
  renderNewMaze(currentLv);
  curX = startX;
  curY = startY;
  matrix.drawPixel(startX,startY,matrix.color565(colors[GREEN][0], colors[GREEN][1]), colors[GREEN][2]);
  matrix.show();
}

void removeGate(int gateNum) {
  int coords[2] = gate_coords[currentLv][gateNum];
  bool RIGHT = false;
  switch (maze[coords[1]][coords[0]+1]) {
    case 3:
      RIGHT = true;
      break;
    default:
      break;
  }
  if (RIGHT)
    //horizontal gate
    for (int i = 0; maze[coords[1]][coords[0]+i] == RED; i++) {
        maze[coords[1]][coords[0]+i] = -3;
        matrix.drawPixel(coords[0]+i,coords[1], matrix.color565(0,0,0));
    }


  else
    //vertical gate
      for (int i = 0; maze[coords[1]+i][coords[0]] == RED; i++) {
            maze[coords[1]+i][coords[0]] = -3;
            matrix.drawPixel(coords[0]+i,coords[1], matrix.color565(0,0,0));
       }
}
