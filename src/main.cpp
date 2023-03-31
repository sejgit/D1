/*
 *  Awning control
 *
 *  Written for an ESP8266 Sonoff Wifi switch
 *  --fqbn esp8266:8266:generic
 *  remember to put in program mode with BTN2 on power-up
 *
 * using the aREST Library for the ESP8266 WiFi chip.
 *
 * init SeJ <2022-12-23 Fri> bring in aREST example and modify
 */

// Import required libraries
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <aREST.h>

#include <ArduinoOTA.h>
#include <String.h>
#include <string>

/* Passwords & Ports
 * wifi: ssid, password
 * ISY: hash, eisy, polisy
 * MQTT mqtt_server, mqtt_serverport
 */
#include </Users/stephenjenkins/Projects/keys/sej/sej.h>

ESP8266WiFiMulti WiFiMulti;

// Create aREST instance
aREST rest = aREST();

// The port to listen for incoming TCP connections
#define LISTEN_PORT 80

// Create an instance of the server
WiFiServer server(LISTEN_PORT);

// timers
unsigned long currentMillis = 0;
unsigned long hbMillis = 0;
const long hbInterval = 60000; // how often to send hb
unsigned long ledMillis = 0;
const long ledInterval = 3000; // blink led h
bool ledState = false;
// awning
unsigned long relayMillis = 0;
const long relayInterval = 30000; // update relay at least this often
int awningpushtime = 2000;

// IO
int pbpower = 12;   // Pin D2 GPIO 0 12
int pbbutton = 13;  // Pin D3 GPIO 4 13
int statusled = 16; // status led
// LED_BUILTIN

// Variables to be exposed to the REST API
boolean heartbeat;
String sent;
int response;
String responseERR;

// variables in the EISY API
std::string serverPath = "http://" + std::string(eisy) + std::string(":") +
                         std::to_string(isyport) + std::string("/rest/nodes");
std::string heartbeatPathDON = serverPath + "/n010_20/cmd/DON";
std::string heartbeatPathDOF = serverPath + "/n010_20/cmd/DOF";

/*
 * Awning trigger
 */
int trigger(String command) {
  // Get state from command
  int state = command.toInt();
  Serial.println(state);
  if (state == 100) {
    Serial.println("request awning trigger");
    // Awning Control only so often
    if (currentMillis - relayMillis > relayInterval) {
      digitalWrite(pbpower, HIGH);
      digitalWrite(pbbutton, HIGH);
      Serial.println("**awning button pushed**");
      delay(awningpushtime);

      digitalWrite(pbbutton, LOW);
      digitalWrite(pbpower, LOW);
      Serial.println("**awning button released**");
      relayMillis = currentMillis;
    } else {
      Serial.println("awning control too soon");
    }
  }
  return state;
}

void setup(void) {
  // Start Serial
  Serial.begin(115200);
  delay(10);

  Serial.println("Boot Start.");

  // Init variables and expose them to REST API
  heartbeat = 0;
  sent = String("init");
  response = 0;
  responseERR = "---";
  rest.variable("heartbeat", &heartbeat);
  rest.variable("sent", &sent);
  rest.variable("response", &response);
  rest.variable("responseERR", &responseERR);

  // Function to be exposed
  rest.function("trigger", trigger);

  // prepare GPIO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,
               HIGH); // Turn the LED off by making the voltage HIGH
  pinMode(pbpower, OUTPUT);
  digitalWrite(pbpower, LOW);
  delay(1);
  pinMode(pbbutton, OUTPUT);
  digitalWrite(pbbutton, LOW);
  delay(1);

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name((char *)"awning");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.println(WiFi.localIP());
  WiFi.setAutoConnect(true);
  WiFi.persistent(true);

  // server start
  server.begin();
  Serial.println("Server started");

  // OTA set-up
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Awningesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  // OTA
  ArduinoOTA.begin();
  Serial.println("OTA Ready.");

  Serial.println("Boot complete.");
  delay(1000);
}

void loop() {
  currentMillis = millis();

  if (WiFiMulti.run() == WL_CONNECTED) {

    // Handle REST calls
    WiFiClient client = server.available();
    WiFiClient rclient;
    HTTPClient http;
    WiFiClientSecure sclient;

    // software interrupts
    ArduinoOTA.handle();
    rest.handle(client);

    // Heartbeat
    if (currentMillis - hbMillis > hbInterval) {
      hbMillis = currentMillis;
      heartbeat = not(heartbeat);

      if (heartbeat == true) {
        sent = String(heartbeatPathDON.c_str());
      } else {
        sent = String(heartbeatPathDOF.c_str());
      }
      Serial.println(sent);

      http.begin(rclient, sent.c_str());
      // http.setAuthorization(isylogin, isypass);
      http.setAuthorization(hash);

      response = http.GET();
      if (response < 0) {
        responseERR = http.errorToString(response);
      } else {
        responseERR = "---";
      }
      http.getString();
      http.end();
      delay(1000);
    }
  }
}
