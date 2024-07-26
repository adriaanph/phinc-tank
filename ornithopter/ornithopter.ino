/**
 * A WiFi-controllable ornithopter, made with an ESP-01 and two micro servos.
 * Have servos wired to GND & 3V3; but only AFTER POWER UP should you connect SERVO_L_PIN & SERVO_R_PIN!
 *
 * adriaan.peenshough@gmail.com
 */
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Hard-code the WiFi & http server name, for clients to connect to (set up with no password)
#define SERVER_ID "ornithopter" // WiFi SSID as well as web server name
#define WIFI_CHANNEL 1 // Static channel, hopefully not congested!

#define GPIO00 0 // Use for output (power up fails if this is LOW)
#define GPIO01_TX 1 // Use for output from chip (default serial transmit; HIGH during power up, fails if this is LOW)
#define GPIO02 2 // == LED_BUILTIN (HIGH during power up, fails if this is LOW)
#define GPIO03_RX 3 // Use for input to the chip (default serial receive; HIGH during power up)

// State machine states. (Types have to be declared before any instance variables!)
enum FLY_STATE {HALTED, CAL_CHECK, READY, FLY_STRAIGHT, FLY_LEFT, FLY_RIGHT};


/** Hardware layer ***********************************************************/
// Pins to connect servos to: note that power up is complicated, so DISCONNECT DURING POWER UP / CONNECT AFTER!
#define SERVO_L_PIN GPIO00 // Pin for left wing
#define SERVO_R_PIN GPIO02 // Pin for right wing

// Servo set point values are encoded as pulse widths, outside of this range some servos "wrap" or start continuous rotation!
// FIND YOUR OWN BY TRIAL & ERROR! Arduino defaults to 544 - 2400 microsec (DEFAULT_MIN_PULSE_WIDTH & DEFAULT_MAX_PULSE_WIDTH), ESP8266 to 1000 - 2000 microsec.
/** For TowerPro SG9 (9gram) 
#define SERVO_L_MINMICROS 700 // 700 - 2300 ~ 120deg
#define SERVO_L_MAXMICROS 2300
#define SERVO_R_MINMICROS 700
#define SERVO_R_MAXMICROS 2300
#define WING_PERIOD_MIN 225 // [millisec/flap(100% to -100% back to 100%)] typical for 9gr servos @3.7V is 0.15sec/60deg; so 0.15sec/60deg*(90deg/200%) = 0.225sec/200%
*/

/** For AGFRC C02 (2gram) & C037 (3.7gram) */
#define SERVO_L_MINMICROS 700 // 2gram: 700 - 2300 ~ 150deg
#define SERVO_L_MAXMICROS 2300
#define SERVO_R_MINMICROS 700 // 3.7gram: 700 - 2300 ~ 150deg
#define SERVO_R_MAXMICROS 2300
#define WING_PERIOD_MIN 120 // [millisec/flap(100% to -100% back to 100%)] typical for 3.7gr servos @3.7V is 0.08sec/60deg; so 0.08sec/60deg*(90deg/200%) = 0.12sec/200%


#define WING_L_MICROS(pct) map(pct,-100,100, SERVO_L_MINMICROS,SERVO_L_MAXMICROS) // percentage [-100..100] => servo pulse microsec
#define WING_R_MICROS(pct) map(pct,-100,100, SERVO_R_MAXMICROS,SERVO_R_MINMICROS) // This one goes the opposite way

Servo wing_L = Servo();
Servo wing_R = Servo();


void set_servos(int pos_L, int pos_R, int tilt) {
  /** Sets both wing positions to match the requested positions.
   * NB: Automatically attaches servos if necessary.
   *
   * @param pos_L, pos_R: positions to go to [+/-percentage].
   * @param tilt: offset to add to L & subtract from R [+/-percentage]
   */
  if (!wing_L.attached() | !wing_R.attached()) {
    wing_L.attach(SERVO_L_PIN, SERVO_L_MINMICROS, SERVO_L_MAXMICROS);
    wing_R.attach(SERVO_R_PIN, SERVO_R_MINMICROS, SERVO_R_MAXMICROS);
  }
  wing_L.writeMicroseconds(WING_L_MICROS(pos_L+tilt));
  wing_R.writeMicroseconds(WING_R_MICROS(pos_R-tilt));
}


unsigned long T_phase0 = millis(); // The reference value of `millis()` that corresponds to phase = 0

void flap_servos(unsigned int ampl_L, unsigned int ampl_R, int tilt, unsigned int cycle_time) {
  /** Continuously (adjusted once per `loop()`) cycle the servos down -> up -> down, following a sinusoidal curve
   * to minimise the "jerkiness".
   * This uses the global `T_phase0` to keep the motion changing smoothly over time.
   *
   * @param ampl_L, ampl_R: amplitudes for the left and right servos [percentage].
   * @param tilt: offset to add to L & subtract from R [+/-percentage]
   * @param cycle_time: duration for a complete flapping cycle [millisec]
   */
  unsigned long T_now = millis();
  float phase_frac = (T_now - T_phase0) /(float) cycle_time; // Fraction of cycles that have passed since T_phase0
  float curve = cos(phase_frac*2*PI);
  set_servos(curve*ampl_L, curve*ampl_R, tilt);

  if (phase_frac > 1) { // Update T_phase0 to prevent the numbers from becoming unnecesssarily big
    T_phase0 = T_phase0 + trunc(phase_frac)*cycle_time; // Adding an integer number of full cycles to remain at "phase 0"
  }
}


/** State Machine ************************************************************/
/** Current state variables */
FLY_STATE current_state;
int wing_tilt; // [percentage] amplitude to tilt the wings (+ & -) to turn
unsigned int wing_ampl; // [percentage] wing amplitude
unsigned int wing_time; // [millisec] duration of an up/downward stroke


void set_defaults() {
  wing_tilt = 0;
  // These should be in the middle of the range
  wing_ampl = 50; // 1/2 of max flapping amplitude
  wing_time = 3 * WING_PERIOD_MIN*(wing_ampl/(float)100); // 1/3rd of fastest flapping rate for chosen amplitude
}


FLY_STATE apply_state(FLY_STATE state, char request) {
  /** Implement the current state & determine which state is next.
   * @return: next state
   */
  FLY_STATE next_state = check_state_transition(state, state, request); // The current state, unless explicitly changed by input

  if (next_state == HALTED) { // Ensure servos are powered down e.g. to minimise risk of damage
    wing_L.detach(); wing_R.detach();
  }
  else if (next_state == CAL_CHECK) { // Allow inspection of ranges & adjustment of servo horns for wings fully UP & fully DOWN
    set_servos(100, 100, 0);
    delay(2*1000); // 2 sec to turn & for inspection
    set_servos(-100, -100, 0);
    delay(1000); // Just enough time to get there before halting (don't block unnecessarily long).
    next_state =  HALTED; // Halt (wings in last position) to make it possible to adjust wings
  }
  else if (next_state == READY) { // Wings locked in current neutral position e.g. when gliding forward, left or right
    set_servos(0, 0, wing_tilt);
  }
  else { // FLY_...: flap the wings as currently configured
    flap_servos(wing_ampl, wing_ampl, wing_tilt, wing_time);
  }
  
  return next_state;
}

FLY_STATE check_state_transition(FLY_STATE current_state, FLY_STATE proposed_next, char request) {
  /** Interpret request (invalid requests are ignored!), potentially changing the next state & variables.
   * Uses the gamers' keypad AWSD, A|a for left, D|d for right, S|s for straight, space or _ for ready/glide.
   * W|w for faster flapping (& straight) Z|X for slower (& straight), + & - to increase & decrease amplitude.
   * C to reset values & perform a calibration check.
   *
   * @param request: the user request (ignored if not in valid set).
   * @return: next state
   */
  FLY_STATE next = proposed_next;

  if (request == 'H' | request == 'h') { // Reset to defaults & halt
    next = HALTED;
  }
  else if (request == 'A' | request == 'a') {
    next = FLY_LEFT;
    wing_tilt = -20; // Left wing lower, right wing higher
  }
  else if (request == 'S' | request == 's') {
    next = FLY_STRAIGHT;
    wing_tilt = 0;
  }
  else if (request == 'D' | request == 'd') {
    next = FLY_RIGHT;
    wing_tilt = 20; // Left wing higher, right wing lower
  }
  else if (request == ' ' | request == '_') { // Wings ready in neutral position - e.g. for glide
    next = READY;
  }
  else if (request == 'C' | request == 'c') { // Set defaults & enter into wing position calibration check
    next = CAL_CHECK;
    set_defaults();
  }
  else if (request == 'W' | request == 'w') { // Decrease wing rest interval
    wing_time = 0.9*wing_time;
  }
  else if (request == 'Z' | request == 'z' | request == 'X' | request == 'x') { // Increase wing rest interval
    wing_time = 1.1*(wing_time+5); // +5 to avoid "lock in" when it's zero
  }
  else if (request == '+') { // Increase wing amplitude
    wing_ampl = 1.1*(wing_ampl+5);
  }
  else if (request == '-') { // Decrease wing amplitude
    wing_ampl = 0.9*wing_ampl;
  }

  return next;
}


/** User Interface layer *****************************************************/
int input; // Input received by input handler in current cycle


void ui_serial_init() {
  /** Command interface through Serial port */
  Serial.println("NB: If you didn't see this message on start up, ensure your servo pins");
  Serial.println("are disconnected on start up & reset! You may now connect your servo pins.\n");

  Serial.println("Use A & D to bank left & right, S for forward and 'Space bar' for glide/rest.");
  Serial.println("Use W for faster and Z (also X) to slow down, + & - increase & decrease the wing amplitude.");
  Serial.println("Use C to reset vlues and perform a cal check and finally, H to halt.");
}

void ui_serial_handleClient() {
  input = Serial.read(); // -1 if nothing available
}


ESP8266WebServer ui_webserver(80); // HTTP (web) server on default port

void ui_webserver_init() {
  /** Command interface trough WiFi HTTP server */
  WiFi.softAP(SERVER_ID, NULL, WIFI_CHANNEL); // NULL -> no password to connect by WiFi
  ui_webserver.on("/", ui_webserver_display);
  ui_webserver.on("/cmd", ui_webserver_cmd);
  ui_webserver.on("/query", ui_webserver_query);
  ui_webserver.begin();
  Serial.print("Web server started at IP "); Serial.println(WiFi.softAPIP());
}

String EOT = "\r\n\r\n"; // It seems to be required to send this, for ESP to close client connections & avoid choking on connections? (Danois90 on forum.arduino.cc)

void ui_webserver_display() {
  /** Display the instructions & command elements on the interface. */
  String script = "<script type=\"text/javascript\">";
  // timeout=2000millisec because the wing flapping cycles are blocking, slowest probably 0.5flap/sec, and "cal check" blocks for ~ 2sec.
  // Send a command without bothering with any response.
  script += "function send(cmd) {";
  script += "var req = new XMLHttpRequest(); req.open(\"PUT\", \"cmd?\"+cmd); req.timeout=2000; req.send(null);";
  script += "}";
  // Queries: there must be an HTML element with ID==query and it must have an explicit 'close' tag!
  script += "function retrv(query) {";
  script += "var req = new XMLHttpRequest(); req.onreadystatechange = function() {";
  script += "  if (this.readyState==4 && this.status==200) { x=document.getElementById(query); if (x!=null) x.innerHTML=this.responseText; }";
  script += "  }; req.open(\"GET\", \"query?\"+query+\"=1\"); req.timeout=2000; req.send(null);";
  script += "}";
  // Queries that are run at regular intervals
  script += "setInterval(function(){retrv('status');}, 3000);";
  script += "setInterval(function(){retrv('rssi');}, 4000);";
  script += "</script>";

  String style = "<head><style>";
  style += "body, a {font-size:55px;}";
  style += "table {display:inline;}"; // No line breaks before & after tables
  style += "button {font-size:55px; line-height:155px}"; // Big buttons
  style += ".panel {line-height:200px; text-align:center; border:10px outset lightblue; border-radius:40px;}"; // Lot of space between buttons
  style += "</style></head>";
  
  String body = "<body><h1 align=\"center\">Ornithopter Contrl</h1><div class=\"panel\">";
  body += "<button onclick=\"send('W');\">FASTER</button> <table>";
  body += "   <tr><td><a onclick=\"send('%2B');return false;\">A+</a></td></tr>";
  body += "   <tr><td><a onclick=\"send('%2D');return false;\">A-</a></td></tr></table> <br/>";
  body += "<button onclick=\"send('A');\">LEFT</button> <button onclick=\"send('S');\">FWD</button> <button onclick=\"send('D');\">RIGHT</button> <br/>";
  body += "<button onclick=\"send('Z');\">SLOWER</button> <button onclick=\"send('_');\">GLIDE</button> <br/>";
  body += "<table><tr><td id='rssi'><td/> <td></td> <td id='status'></td></table> <br/>";
  body += "<br/>";
  body += "<button onclick=\"send('H');\">HALT</button> <br/>";
  body += "<button onclick=\"send('C');\">CAL CHECK</button>";
  body += "</div></body>";
  
  ui_webserver.send(200, "text/html", script + style + body + EOT);
}

void ui_webserver_cmd() {
  /** Handles command requests. Note this does not send any response back to update the interface. */
  if (ui_webserver.args() == 1) {
    input = ui_webserver.argName(0)[0]; // Only expect a single character
  }
  ui_webserver.send(200, "text/plain", EOT);
}

void ui_webserver_query() {
  /** Handles query requests, sends plain text back for browser script to update the interface (no page reload). */
  if (ui_webserver.args() == 1) {
    String query = ui_webserver.argName(0);
    // Code to respond to query
    if (query == "rssi") {
      String ss = String(WiFi.RSSI()) + "dBm";
      ui_webserver.send(200, "text/plain", ss+EOT);
    }
    else if (query == "status") {
      String status = "+/-" + String(wing_ampl) + "%";
      status += " @ " + String(wing_time) + "ms";
      ui_webserver.send(200, "text/plain", status+EOT);
    }
  }
}

/** Processor layer **********************************************************/

void setup() {
  Serial.begin(9600);
  // Initialise the command interface(s) and pick the start up state.
  ui_serial_init();
  ui_webserver_init();
  // Start-up defaults
  set_defaults();
  current_state = HALTED; // Prefer to start in HALTED to ensure servos are de-powered.
}


void loop() {
  // As long as power is on, read requests and run the state machine.
  input = -1;
  ui_serial_handleClient();
  ui_webserver.handleClient();

  FLY_STATE next_state = apply_state(current_state, input);
  if (next_state != current_state) { // DEBUGGING
    Serial.print("State changed from "); Serial.print(current_state); Serial.print(" to "); Serial.println(next_state);
  }
  else if (input > 0) {
    Serial.print("  wing_ampl="); Serial.print(wing_ampl); Serial.print(", wing_time="); Serial.println(wing_time);
  }
  current_state = next_state;
}