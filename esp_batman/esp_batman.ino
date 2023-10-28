/**
 * A very basic ESP8266 WiFi controller with simple web GUI.
 *
 * adriaan.peenshough@gmail.com (framework)
 * arami.peenshough@gmail.com (web GUI)
 * benjamin.peenshough@gmail.com (hardware)
 */
#include <WiFi.h>
#include <WebServer.h>

// ADC2 pins cannot be used when Wi-Fi is used, so can only use pins 32 - 39 [https://randomnerdtutorials.com/esp32-pinout-reference-gpios]
#define BAT_PINA 34 // CAUTION: max 3.5V!!!!
#define BAT_PINB 35
// An output pin - for demonstration
#define OUT_PIN 23

// These details are used first for the ESP to attempt joining an existing WiFi network
#define JOIN_WIFI_ID "vodafoneBBB2E4"
#define JOIN_WIFI_PSWD "Arami&Benjamin"
// These details are used if the ESP needs to create a stand-alone WiFi access point for clients to connect to
#define WIFI_ID "esp-batman" // WiFi SSID as well as web server name
#define WIFI_PSWD NULL // No password required to connect!
#define WIFI_CHANNEL 1 // Static channel, hopefully not congested...


WebServer ui_webserver(80); // HTTP (web) server on default port

String cmd; // Command received by input handler in current cycle


void ui_webserver_init() {
  /** Command interface trough WiFi HTTP server */
  Serial.println();

  // First try to connect to existing WiFi network
  bool standalone = false;
  WiFi.begin(JOIN_WIFI_ID, JOIN_WIFI_PSWD);
  Serial.print("Waiting 5 seconds to join WiFi network ...");
  WiFi.waitForConnectResult(5000);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" connected!");
    standalone = false;
  } else { // Failed to join
    Serial.println(" FAILED!");
    WiFi.disconnect();

    // Create a standalone network
    Serial.print("Starting a standalone WiFi network ...");
    if (WiFi.softAP(WIFI_ID, WIFI_PSWD, WIFI_CHANNEL)) {
      Serial.println(" started!");
    } else {
      Serial.print(" FAILED!");
      Serial.print("FATAL ERRROR: Failed to bring up WiFi interface!");
      while (true) {} // Hang the processor
    }
    standalone = true;
  }

  ui_webserver.on("/", ui_webserver_display);
  ui_webserver.on("/cmd", ui_webserver_cmd);
  ui_webserver.on("/query", ui_webserver_query);
  ui_webserver.begin();
  
  Serial.print("Web server started at IP ");
  if (standalone) {
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(WiFi.localIP());
  }
}

void ui_webserver_display() {
  /** Display the instructions & command elements on the interface. */
  String script = "<script type=\"text/javascript\">";
  // timeout=100millisec is my workaround for bug <https://www.tablix.org/~avian/blog/archives/2022/08/esp8266_web_server_is_slow_to_close_connections> - timouts in browser console are harmless
  // Commands:
  script += "function send(cmd) {";
  script += "var req = new XMLHttpRequest(); req.open(\"POST\", \"cmd?\"+cmd+\"=1\"); req.timeout=100; req.send(null);";
  script += "}";
  // Queries: there must be an HTML element with ID==query and it must have an explicit 'close' tag!
  script += "function retrv(query) {";
  script += "var req = new XMLHttpRequest(); req.onreadystatechange = function() {";
  script += "  if (this.readyState==4 && this.status==200) { x=document.getElementById(query); if (x!=null) x.innerHTML=this.responseText; }";
  script += "  }; req.open(\"GET\", \"query?\"+query+\"=1\"); req.timeout=100; req.send(null);";
  script += "}</script>";

  String style = "<head><style>";
  style += "body {font-size:55px;}";
  style += ".bigbutton {font-size:55px; line-height:155px}"; // Big buttons
  style += ".smolbutton {font-size:25px; line-height:55px}"; // smol buttons
  style += ".panel {line-height:200px; text-align:center; border:10px outset lightblue; border-radius:40px;}"; // Lot of space between buttons
  style += "td {text-align:center;font-size:55px}"; // Default table cell alignment
  style += "</style></head>";
  
  String body = "<body><h1 align=\"center\">BatMan (V 1.Pune)</h1>";
  body += "<div class=\"panel\"><table align=\"center\">";
  // body += "<tr> <td><button onclick=\"send('OUT=1');\">ON</button></td> <td/> <td><button onclick=\"send('OUT=0');\">OFF</button></td> </tr>";
  // body += "<tr/>";
  body += "<tr> <td><button class='bigbutton'>Bat A Voltage</button></td> <td><button class='smolbutton'>&nbsp</button></td> <td id='BATAmV'> </td> <td>mV</td> </tr>";
  body += "<tr> <td><button class='bigbutton'>Bat B Voltage</button></td> <td><button class='smolbutton'>&nbsp</button></td> <td id='BATBmV'> </td> <td>mV</td> </tr>";
  body += "</table></div></body>";
  
  // Queries that are run at regular intervals
  String auto_update = "<script type=\"text/javascript\">setInterval(function(){retrv('BATAmV');}, 750);</script>";
          auto_update += "<script type=\"text/javascript\">setInterval(function(){retrv('BATBmV');}, 750);</script>";

  ui_webserver.send(200, "text/html", script + style + body + auto_update);
}

void ui_webserver_cmd() {
  /** Handles command requests, doesn't send anything back to change the interface. */
  if (ui_webserver.args() == 1) {
    cmd = ui_webserver.argName(0);
    String val = ui_webserver.arg(0);
    // Code to do something with the command
    Serial.print("Got command: "); Serial.println(cmd);
    if (cmd == "OUT") {
      if (val[0] == '0') {
        digitalWrite(OUT_PIN, 0);
      } else if (val[0] == '1') {
        digitalWrite(OUT_PIN, 1);
      }
    }
  }
}

void ui_webserver_query() {
  /** Handles query requests, sends plain text back for browser script to update the interface (no page reload). */
  if (ui_webserver.args() == 1) {
    String query = ui_webserver.argName(0);
    // Code to respond to query
    Serial.print("Got query: "); Serial.println(query);
    if (query == "BATBmV") { 
      int mV = analogReadMilliVolts(BAT_PINB);
      ui_webserver.send(200, "text/plain", String(mV));
    } else if (query == "BATAmV") { // Battery pin milliVolt
      int mV = analogReadMilliVolts(BAT_PINA);
      ui_webserver.send(200, "text/plain", String(mV));
    }
  }
}


void handle_internals() {
  /** Things that need to happen once per loop, regardless of user input. */
  // NB: this must not take more than ~ 100ms!
}


void setup() {
  // Enable debugging via serial monitor
  Serial.begin(9600);
  // Configure the web server for User Interface
  ui_webserver_init();
  // Configure the input & outputs
  pinMode(BAT_PINA, INPUT);
  pinMode(BAT_PINB, INPUT);
  pinMode(OUT_PIN, OUTPUT);
}


void loop() {
  cmd = "";
  ui_webserver.handleClient();
  handle_internals();
}