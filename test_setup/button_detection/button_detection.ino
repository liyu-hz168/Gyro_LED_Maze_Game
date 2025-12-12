#define BUTTON_PIN 9 //D9
void setup()
{
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}
void loop()
{
  Serial.println(digitalRead(BUTTON_PIN));
  delay(100);
}
