/**
 * A very basic ESP8266 WiFi controller with simple web GUI.
 * Note: Simply delete the "ESP8266" prefixes everywhere below to use e.g. ESP32.
 *
 * adriaan.peenshough@gmail.com
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// These details are used first for the ESP to attempt joining an existing WiFi network
#define JOIN_WIFI_ID "existing_wifi_ssid"
#define JOIN_WIFI_PSWD "existing_wifi_password"
// These details are used if the ESP needs to create a stand-alone WiFi access point for clients to connect to
#define SERVER_ID "esp-control" // WiFi SSID as well as web server name
#define SERVER_PSWD NULL // No password required to connect!
#define WIFI_CHANNEL 1 // Static channel, hopefully not congested...


ESP8266WebServer ui_webserver(80); // HTTP (web) server on default port

char cmd; // Command received by input handler in current cycle


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
  script += "var req = new XMLHttpRequest(); req.open(\"POST\", \"cmd?\"+cmd); req.timeout=100; req.send(null);";
  script += "}";
  // Queries: there must be an HTML element with ID==query and it must have an explicit 'close' tag!
  script += "function retrv(query) {";
  script += "var req = new XMLHttpRequest(); req.onreadystatechange = function() {";
  script += "  if (this.readyState==4 && this.status==200) { x=document.getElementById(query); if (x!=null) x.innerHTML=this.responseText; }";
  script += "  }; req.open(\"GET\", \"query?\"+query); req.timeout=100; req.send(null);";
  script += "}</script>";

  String style = "<head><style>";
  style += "body {font-size:55px;}";
  style += "button {font-size:55px; line-height:155px}"; // Big buttons
  style += ".panel {line-height:200px; text-align:center; border:10px outset lightblue; border-radius:40px;}"; // Lot of space between buttons
  style += "td {text-align:center;}"; // Default table cell alignment
  style += "</style></head>";
  
  String body = "<body><h1 align=\"center\">Control Panel</h1>";
  body += "<div class=\"panel\"><table align=\"center\">";
  body += "<tr> <td/> <td><button onclick=\"send('W');\">FWD</button></td> <td/> </tr>";
  body += "<tr> <td><button onclick=\"send('A');\">LEFT</button></td> <td><button onclick=\"send('S');\">STOP</button></td> <td><button onclick=\"send('D');\">RIGHT</button></td> </tr>";
  body += "<tr/>";
  body += "<tr> <td><button onclick=\"retrv('M');\">GET M</button></td> <td id='M'></td> <td id='T'></td> </tr>";
  body += "</table></div></body>";
  
  // Queries that are run at regular intervals
  String auto_update = "<script type=\"text/javascript\">setInterval(function(){retrv('T');}, 2000);</script>";

  ui_webserver.send(200, "text/html", script + style + body + auto_update);
}

void ui_webserver_cmd() {
  /** Handles command requests, doesn't send anything back to change the interface. */
  if (ui_webserver.args() == 1) {
    cmd = ui_webserver.argName(0)[0]; // Change this to work with more than single character inputs
    // Sample code to do something with the command
    Serial.print("Got command: "); Serial.println(cmd);
    if (cmd == 'W') {
    }
  }
}

void ui_webserver_query() {
  /** Handles query requests, sends plain text back for browser script to update the interface (no page reload). */
  if (ui_webserver.args() == 1) {
    char query = ui_webserver.argName(0)[0]; // Change this to work with more than single character inputs
    // Sample code to respond to query
    Serial.print("Got query: "); Serial.println(query);
    if (query == 'T') {
      ui_webserver.send(200, "text/plain", String(millis()));
    } else {
      ui_webserver.send(200, "text/plain", "OK!");
    }
  }
}


void setup() {
  Serial.begin(9600);
  ui_webserver_init();
}


void loop() {
  cmd = 0;
  ui_webserver.handleClient();
  // Prefer to only act on commands in "ui_webserver_cmd", not here
}