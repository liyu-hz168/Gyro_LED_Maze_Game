# Gyro-Controlled LED Maze Game 
A tilt-controlled LED maze game implemented using an Arduino Nano 33 BLE Sense and an ESP32. The Arduino streams real-time accelerometer data over Bluetooth, 
while the ESP32 handles game logic and renders gameplay on a 32×32 RGB LED matrix. Audio was prototyped but removed due to Bluetooth timing interference. This project was developed 
as a final project for CS 335 - Inside the Box: How Computers Work.
# Demo 
**Controlling the player:**

https://github.com/user-attachments/assets/3b0fe484-03ed-467f-ba43-3f5b98b45b97  


**Getting key -> Unlock door:**

https://github.com/user-attachments/assets/de36bb2f-6f1a-49fa-bbf3-4daa5599a7a6  


**Reaching destination -> Win state -> Next Level:**

https://github.com/user-attachments/assets/220c5b99-9c93-4803-8662-14dae397e638  


**Blinking blue walls:**

https://github.com/user-attachments/assets/6d63cc51-76a7-4f44-a48a-a2b6654cc52d


**Colliding with red walls -> Death state:**

https://github.com/user-attachments/assets/00679928-4a08-4b9e-8267-90c315022300

(Thank you, Sophia, for being our first ever gameplay tester 🙏)

# Game Logic Overview 
### Player Movement
 The Arduino reads accelerometer data from its onboard IMU.
   - Tilt values (X and Y axes) are scaled, dead-zoned, and sent via Bluetooth to the ESP32.
   - The ESP32 updates the player's position on the LED matrix accordingly, moving the player smoothly while following collision rules.

### Collision Types
| Tile Type | Color | Behavior |
|-----------|-------|----------|
| Standard Wall | Magenta | Solid wall; player cannot pass through |
| Death Wall | Red | Instant death on contact |
| Flashing Wall | Blue | Turns on and off, solid when on; lethal if player is standing when it turns back on |
| End Zone | Yellow | Triggers level completion / win condition |
| Key Pickup | Gold | Opens corresponding gates when collected |
     
### Reset Button 
The Arduino has a dedicated reset button.
   - Pressing it sends a reset signal to the ESP32, returning the player to the first level and the initial position.

# Installation & Setup
....Will eventually finish writing this
### Wiring 
### Library 
# Known Issues & Limitations
- Audio system was scrapped due to Bluetooth overhead, causing low sample rate playback (Sounded like haunted house music...)
- Adafruit_Protomatter is incompatible with the Nano 33 BLE Sense due to the missing `g_Adigitalpinmap` array in newer MBED OS versions





