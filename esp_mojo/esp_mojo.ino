/**
 * Simple app for ESP32, to turn some LEDs on.
 */
#define LEN(a)  (sizeof(a)/sizeof(a[0]))
uint8_t LED_PINS[] = {12, 27, 25}; // Pins where LEDs are attached
// TODO: Some un-anticipated consequences - these pins seem to interact (can't be HIGH together)


// the setup function runs once when you press reset or power the board
void setup() {
  for (uint8_t i=0; i<LEN(LED_PINS); i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
}

// the loop function runs over and over again forever
void loop() {
  for (uint8_t i=0; i<LEN(LED_PINS); i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(1000);
  }

  for (uint8_t i=0; i<LEN(LED_PINS); i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  delay(1000);
}
