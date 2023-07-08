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
#define SERVO_L_MINMICROS 700 // 700 - 2300 ~ 90deg
#define SERVO_L_MAXMICROS 2300
#define SERVO_R_MINMICROS 700
#define SERVO_R_MAXMICROS 2300
#define WING_RATE_MAX 900 // [percent/sec] typical for 9gr servos @3.7V is 0.15sec/60deg; so 0.15sec/60deg*(90deg/200%) = 0.225sec/200% --> 889%/sec
*/

/** For AGFRC C02 (2gram) & C037 (3.7gram) */
#define SERVO_L_MINMICROS 700 // 2gram: 700 - 2300 ~ 150deg
#define SERVO_L_MAXMICROS 1700
#define SERVO_R_MINMICROS 700 // 3.7gram: 700 - 2300 ~ 150deg
#define SERVO_R_MAXMICROS 1700
#define WING_RATE_MAX 1700 // [percent/sec] typical for 3.7gr servos @3.7V is 0.08sec/60deg; so 0.08sec/60deg*(90deg/200%) = 0.12sec/200% --> 1667%/sec


#define WING_L_MICROS(pct) map(pct,-100,100, SERVO_L_MINMICROS,SERVO_L_MAXMICROS) // percentage [-100..100] => servo pulse microsec
#define WING_R_MICROS(pct) map(pct,-100,100, SERVO_R_MAXMICROS,SERVO_R_MINMICROS) // This one goes the opposite way

Servo wing_L = Servo();
Servo wing_R = Servo();


void set_servos(int pos_L, int pos_R) {
  /** Sets both wing positions to match the requested positions.
   * NB: Automatically attaches servos if necessary.
   * @param pos_L, pos_R: [+/-percentage].
   */
  if (!wing_L.attached() | !wing_R.attached()) {
    wing_L.attach(SERVO_L_PIN, SERVO_L_MINMICROS, SERVO_L_MAXMICROS);
    wing_R.attach(SERVO_R_PIN, SERVO_R_MINMICROS, SERVO_R_MAXMICROS);
  }
  wing_L.writeMicroseconds(WING_L_MICROS(pos_L));
  wing_R.writeMicroseconds(WING_R_MICROS(pos_R));
}


/** State Machine ************************************************************/
/** Current state variables */
FLY_STATE current_state;
unsigned int wing_ampl; // [percentage] wing amplitude
unsigned int wing_rest; // [millisec] to wait after a downward stroke

int WING_REST_MIN() { // [millisec] fastest time for a full swing from +wing_ampl to -wing_ampl
  return (2*wing_ampl*1000) / WING_RATE_MAX;
}

void set_defaults() {
  wing_ampl = 100; // Max flapping amplitude
  wing_rest = WING_REST_MIN(); // Fastest flapping for this amplitude
}


FLY_STATE apply_state(FLY_STATE state, char request) {
  /** Implement the current state & determine which state is next.
   * @return: next state
   */
  FLY_STATE next_state = HALTED; // This should make state machine errors very obvious

  if (state == HALTED) { // Ensure servos are powered down to minimie risk of damage
    wing_L.detach(); wing_R.detach();
    next_state = check_state_transition(state, state, request); // Stay in this state unless requested
  }
  else if (state == CAL_CHECK) { // Allow inspection of ranges & adjustment of servo horns for wings fully UP & fully DOWN
    set_servos(100, 100);
    delay(2*1000); // 2 sec to turn & for inspection
    set_servos(-100, -100);
    delay(2*1000); // 2 sec
    next_state = check_state_transition(state, HALTED, request); // Halt (wings in last position) to make it possible to adjust wings
  }
  else if (state == READY) { // Wings in neutral position e.g. when gliding or ready to start flapping
    set_servos(0, 0);
    next_state = check_state_transition(state, state, request); // Stay in this state unless requested
  }
  else { // FLY_...: Wings cycle down -> up - BLOCKING!
    if (state == FLY_STRAIGHT) { // Wings cycle down -> up
      set_servos(-wing_ampl, -wing_ampl);
    }
    else if (state == FLY_LEFT) { // Left wing only half-way down 
      set_servos(-wing_ampl/2, -wing_ampl);
    }
     else if (state == FLY_RIGHT) { // Right wing only half-way down 
      set_servos(-wing_ampl, -wing_ampl/2);
    }
    delay(wing_rest); // Pause after down stroke
    set_servos(wing_ampl, wing_ampl);
    delay(wing_rest); // Pause after up stroke
    next_state = check_state_transition(state, state, request); // Stay in this state unless requested
  }

  return next_state;
}


FLY_STATE check_state_transition(FLY_STATE current_state, FLY_STATE proposed_next, char request) {
  /** Interpret request (invalid requests are ignored!), potentially changing the next state.
   * Uses the gamers' keypad AWSD, A|a for left, D|d for right, S|s for straight, space or _ for ready/glide.
   * W|w for faster flapping (& straight) Z|X for slower (& straight), + & - to increase & decrease amplitude.
   * C to reset values & perform a calibration check.
   *
   * @param request: the user request (ignored if not in valid set).
   * @return: next state
   */
  FLY_STATE next = proposed_next;

  if (request == 'H' | request == 'h') { // Reset to defaults & halt
    set_defaults();
    next = HALTED;
  }
  else if (request == 'A' | request == 'a') {
    next = FLY_LEFT;
  }
  else if (request == 'S' | request == 's') {
    next = FLY_STRAIGHT;
  }
  else if (request == 'D' | request == 'd') {
    next = FLY_RIGHT;
  }
  else if (request == ' ' | request == '_') { // Wings ready but in neutral position - e.g. for glide
    next = READY;
  }
  else if (request == 'C' | request == 'c') { // Enter into wing position calibration check
    next = CAL_CHECK;
  }
  else if (request == 'W' | request == 'w') { // Decrease wing rest interval
    wing_rest = constrain(wing_rest-20, WING_REST_MIN()/2,WING_REST_MIN()*5);
    next = FLY_STRAIGHT;
  }
  else if (request == 'Z' | request == 'z' | request == 'X' | request == 'x') { // Increase wing rest interval
    wing_rest = constrain(wing_rest+20, WING_REST_MIN()/2,WING_REST_MIN()*5);
    next = FLY_STRAIGHT;
  }
  else if (request == '+') { // Increase wing amplitude
    wing_ampl = constrain(wing_ampl+10, 10,100);
    wing_rest = WING_REST_MIN(); // Fastest flapping for this amplitude
  }
  else if (request == '-') { // Decrease wing amplitude
    wing_ampl = constrain(wing_ampl-10, 10,100);
    wing_rest = WING_REST_MIN(); // Fastest flapping for this amplitude
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
  ui_webserver.begin();
  Serial.print("Web server started at IP "); Serial.println(WiFi.softAPIP());
}

void ui_webserver_display() {
  /** Display the instructions & command elements on the interface. */
  String script = "<script type=\"text/javascript\">function send(cmd) {";
  // timeout=100millisec is my workaround for bug <https://www.tablix.org/~avian/blog/archives/2022/08/esp8266_web_server_is_slow_to_close_connections> - timouts in browser console are harmless
  script += "var req = new XMLHttpRequest(); req.open(\"POST\", \"cmd?\"+cmd); req.timeout=100; req.send(null);";
  script += "}</script>";

  String style = "<head><style>";
  style += "body, a {font-size:55px;}";
  style += "table {display:inline;}"; // No line breaks befor & after tables
  style += "button {font-size:55px; line-height:155px}"; // Big buttons
  style += ".panel {line-height:200px; text-align:center; border:10px outset lightblue; border-radius:40px;}"; // Lot of space between buttons
  style += "</style></head>";
  
  String body = "<body><h1 align=\"center\">Ornithopter Contrl</h1><div class=\"panel\">";
  body += "<button onclick=\"send('W');\">FASTER</button> <table>";
  body += "   <tr><td><a onclick=\"send('%2B');return false;\">A+</a></td></tr>";
  body += "   <tr><td><a onclick=\"send('%2D');return false;\">A-</a></td></tr></table> <br/>";
  body += "<button onclick=\"send('A');\">LEFT</button> <button onclick=\"send('S');\">FWD</button> <button onclick=\"send('D');\">RIGHT</button> <br/>";
  body += "<button onclick=\"send('Z');\">SLOWER</button> <button onclick=\"send('_');\">GLIDE</button> <br/>";
  body += "<br/>";
  body += "<button onclick=\"send('C');\">CAL CHECK</button> <button onclick=\"send('H');\">HALT</button>";
  body += "</div></body>";
  
  ui_webserver.send(200, "text/html", script + style + body);
}

void ui_webserver_cmd() {
  /** Handles command requests. Note this doesn't send anything back to update the interface. */
  if (ui_webserver.args() == 1) {
    input = ui_webserver.argName(0)[0]; // Only expect a single character
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
    Serial.print("  wing_ampl="); Serial.print(wing_ampl); Serial.print(", wing_rest="); Serial.println(wing_rest);
  }
  current_state = next_state;
}