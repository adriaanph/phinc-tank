/**
 * Simply creates a WiFi access point on an ESP-32.
 * This could e.g. connect AirPlay devices.
 * Note: the ESP-01 only supports WiFi 802.11/ac, not /b which must used for e.g. AirPlay
 **/

#include <WiFi.h>
//#include <WiFiClient.h>
#include <WiFiAP.h>

// Set these to your desired credentials.
const char *ssid = "phinc-wifi";
const char *password = "phinc0412";


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP started with IP address: ");
  Serial.println(myIP);
}


void loop() {
  // AP runs in the background
}
