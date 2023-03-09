/**
 * Simple app for ESP32, to:
 *   1) turn some LEDs on and
 *   2) fiddle with a servo.
 * See comments in the code for how to connect things up.
 */
#include <ESP32Servo.h>
#define LEN(a)  (sizeof(a)/sizeof(a[0]))


/**** All of the servo-specific code ****/
Servo servo1;
int SERVO1_PIN = 18; // Pin where the servo's control line is attached. PWM GPIO pins on the ESP32 include 2,4,12-19,21-23,25-27,32-33.
int servo1_angle = 0; // Keeps track of the servo's current commanded position

void setupServos() {
	// This is something you just have to do when using the ESP32Servo library
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);

	servo1.setPeriodHertz(50);    // Standard servos work with 50 pulses per second
	servo1.attach(SERVO1_PIN, 600, 2300); // Widest possible range is 500 to 2500microsec i.e. ..., 500, 2500)
	// Each servo may require a different min/max setting for an accurate 0 to 180 sweep.
}

void wipeServos() {
  if (servo1_angle < 180) {
    for (servo1_angle = 0; servo1_angle <= 180; servo1_angle += 1) { // From 0 to 180degrees in 1deg steps
      servo1.write(servo1_angle);
      delay(10); // Wait 5ms for the servo to reach the position
    }
  }
  else {
    for (servo1_angle = 180; servo1_angle >= 0; servo1_angle -= 1) { // From 180 to 0degrees in 1deg steps
      servo1.write(servo1_angle);
      delay(10);
    }
  }
}


/**** All of the LED-specific code ****/
int LED_PINS[] = {12, 27, 25}; // Pins where LEDs are attached

void setupLEDs() {
  for (int i=0; i<LEN(LED_PINS); i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
}

void flashLEDs() {
  for (int i=0; i<LEN(LED_PINS); i++) { // Turn all off at once
    digitalWrite(LED_PINS[i], LOW);
  }
  delay(1000); // 1000ms

  for (int i=0; i<LEN(LED_PINS); i++) { // Turn them on, one by one
    digitalWrite(LED_PINS[i], HIGH);
    delay(1000); // 1000ms
  }
}



/**** Initialise & run ****/

// The setup function runs once when you press reset or power the board
void setup() {
  setupLEDs();
  setupServos();
}

// The loop function runs over and over again forever
void loop() {
  flashLEDs();
  wipeServos();
}
