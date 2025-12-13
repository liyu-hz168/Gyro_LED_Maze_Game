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

Adafruit_MCP4725 dac;
#define DAC_RESOLUTION    (8)

#define CHIP_SELECT   10          // SD CS pin
#define SAMPLE_RATE   8000UL     //8kHz for MCP4725 over I2C
#define BUFFER_SIZE   512 

File wavFile;
uint8_t  buf[BUFFER_SIZE];
size_t   bufLen = 0;
size_t   bufPos = 0;

unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 1000000UL / SAMPLE_RATE;


void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.print("Initializing SD card...");
  if (!SD.begin(CHIP_SELECT)) {
    Serial.println("SD initialization failed!");
    while (1);
  }
  Serial.println("SD initialized.");

  if (!SD.exists("DEATH.WAV")) {
    Serial.println("File not found!");
    while (1);
  }
  wavFile = SD.open("DEATH.WAV", FILE_READ);
  
  if (!wavFile) {
    Serial.println("Couldn't open WAV file");
    while (1);
  }
  wavFile.seek(44); // Skip WAV header

  Wire.begin();
  //Wire.setClock(400000);                 // fast I2C for higher update rate
  Wire.setClock(10000);
  if (!dac.begin(0x62)) {                // default MCP4725 address is 0x60 on Adafruit board
    Serial.println("MCP4725 not found");
  }
  Serial.println("MCP4725 found!!");

  refillBuffer();
  lastSampleTime = micros();

  Serial.println("Streaming WAV via MCP4725 at 8 kHz...");
}

void loop() {
  // Keep buffer filled
  refillBuffer();

  // Output samples at fixed rate
  unsigned long now = micros();
  if ((now - lastSampleTime) >= sampleInterval) {
    lastSampleTime += sampleInterval;

    if (bufPos < bufLen) {
      uint8_t sample = buf[bufPos++];
      
      dac.setVoltage((uint16_t)map8_to_12_full(sample), false);
      //dac.fastWrite((uint16_t)map8_to_12_full(sample), false);
    } else {
      // Buffer underrun: output midscale briefly
      dac.setVoltage(2048, false);
      //dac.fastWrite(2048, false);
    }
  }
}

// Audio WAV file 8 bits .. too lazy to convert
inline uint16_t map8_to_12_full(uint8_t s) {
// 0..255 -> 0..4095
return (uint16_t)s << 4; // s * 16
}

// Refill the audio buffer from SD when empty
void refillBuffer() {
  const size_t REFILL_THRESHOLD = 128; // start refilling when <128 bytes remain
  if (bufLen - bufPos > REFILL_THRESHOLD) return;
  if (!wavFile) return;
  //if (bufPos < bufLen) return; // still have data

  // Move remaining data to the front of buffer
  size_t remain = bufLen - bufPos;
  if (remain && bufPos) {
    memmove(buf, buf + bufPos, remain);
  }
  bufLen = remain;
  bufPos = 0;

  // Top up to BUFFER_SIZE
  int toRead = (int)(BUFFER_SIZE - bufLen);
  int r = wavFile.read(buf + bufLen, toRead);
  if (r <= 0) {
    // Loop the file
    r = wavFile.read(buf + bufLen, toRead);
    if (r <= 0) return;
  }
  bufLen += (size_t)r;
}

