#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
using namespace websockets;

// Websocket declarations
WebsocketsClient wss_client;
DynamicJsonDocument wss_res(220);
bool wss_msg_received = false;
bool wss_connected = false;

// Static var declarations
const String ssid = "BrewBot";
const String password = "BrewBotHotspot";
bool wifi_connecting = false;

const String wss_host = "ws://145.220.74.183";
const String wss_path = "/socket.io";
const int wss_port = 34535;
const String wss_token = "2ZJQrLFrLB2zvx.4WLqWJZ-MoepVn384hcvDTvCc";

const int sensorPin = 5;
const int pump1Pin = 14;
const int pump2Pin = 12;
const int pump3Pin = 13;
const int ledRedPin = 2;
const int ledGreenPin = 16;
const int ledBluePin = 4;

const int TAP_TIME_MS = 5000;

// Function declarations
bool tab_beer(String type);
void pulseRedLed(int duration);
void pulseGreenLed(int duration);
void pulseBlueLed(int duration);
void sendWssMessage(String action, String type, int user_id);

// Websocket callbacks
void onMessageCallback(WebsocketsMessage message) {
    if(wss_msg_received) return;

    DeserializationError wss_error = deserializeJson(wss_res, message.data());
    if (wss_error) {
        Serial.print(F("Error parsing JSON: "));
        Serial.println(wss_error.c_str());
        wss_msg_received = false;
        return;
    }

    String action = wss_res["action"];
    if(action == "activate") {
      wss_msg_received = true;
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("WSS: Connnection Opened");
        wss_connected = true;
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WSS: Connnection Closed");
        wss_connected = false;
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("WSS: Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("WSS: Got a Pong!");
    }
}

void connectToWiFi() {
  if(wifi_connecting) return;

  wifi_connecting = true;

  //Connect to WiFi Network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to WiFi");
  Serial.println("...");
  WiFi.begin(ssid, password);
  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 1000)) {
    retries++;
    Serial.print(".");
    pulseRedLed(600);
  }
  if (retries > 999) {
    Serial.println(F("WiFi connection FAILED"));
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi connected!"));
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Connected to WiFi Network
    pulseGreenLed(1200);
    wifi_connecting = false;
  }
}

void connectToWss(){
  if(wss_connected) return;

  // Connect to server
  String server_url = wss_host + ":" + wss_port + wss_path;
  wss_client.connect(server_url);
}

// Setup
void setup()
{
  Serial.begin(115200);

  // Define pin modes
  pinMode(sensorPin, INPUT);
  pinMode(pump1Pin, OUTPUT);
  pinMode(pump2Pin, OUTPUT);
  pinMode(pump3Pin, OUTPUT);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  pinMode(ledBluePin, OUTPUT);

  // Turn off pumps
  digitalWrite(pump1Pin, HIGH);
  digitalWrite(pump2Pin, HIGH);
  digitalWrite(pump3Pin, HIGH);

  // Turn off leds
  digitalWrite(ledRedPin, LOW);
  digitalWrite(ledGreenPin, LOW);
  digitalWrite(ledBluePin, LOW);

  // Connect to Wi-Fi
  connectToWiFi();

  // Setup Callbacks
  wss_client.onMessage(onMessageCallback);
  wss_client.onEvent(onEventsCallback);
  
  // Connect to server
  connectToWss();

  Serial.println(F("BrewBot is ready!"));
}

// Loop
void loop() {
  connectToWss();

  wss_client.poll();

  if(WiFi.status() == WL_CONNECTED && wss_connected){
    digitalWrite(ledBluePin, HIGH);
  }else{
    connectToWiFi();
    return;
  }

  if (!wss_msg_received) return;

  String action = wss_res["action"];
  String type = wss_res["type"];
  String to = wss_res["to"];
  int user_id = (int)wss_res["user_id"];

  if (action == "activate" && to == "brewbot_machine_1"){
    int glassPresent = digitalRead(sensorPin);
    while(glassPresent == HIGH)
    {
      glassPresent = digitalRead(sensorPin);
      Serial.println("Waiting for glass");
      pulseBlueLed(1000);
    }

    bool success = tab_beer(type);
    if(success){
      sendWssMessage("completed", type, user_id);
      Serial.println("Beer tapped");
    }

    int glassRemoved = digitalRead(sensorPin);
    while(glassRemoved == LOW)
    {
      glassRemoved = digitalRead(sensorPin);
      Serial.println("Waiting for glass to be removed");
      pulseGreenLed(800);
    }

    Serial.println("Glass removed");
    pulseGreenLed(1000);
    pulseGreenLed(1000);

    wss_msg_received = false;
    digitalWrite(ledBluePin, HIGH);
    Serial.println("Ready for next beer");
  }
}

bool tab_beer(String type)
{
  int beerPin;
  if(type == "zero"){
    beerPin = pump1Pin;
  }else if(type == "normal"){
    beerPin = pump2Pin;
  }else if(type == "special"){
    beerPin = pump3Pin;
  }else{
    Serial.println("Invalid beer type");
    return false;
  }


  digitalWrite(beerPin, LOW);
  Serial.println("Tapping started");

  int blink_time = 300;
  int blink_count = TAP_TIME_MS / blink_time;
  for (int i = 0; i < blink_count; i++)
  {
    pulseBlueLed(blink_time);
    int glassRemoved = digitalRead(sensorPin);
    if(glassRemoved == HIGH){
      Serial.println("Tapping stopped: Glass removed");
      digitalWrite(beerPin, HIGH);

      pulseRedLed(400);
      pulseRedLed(400);
      
      digitalWrite(ledBluePin, HIGH);
      return false;
    }
  }
  
  digitalWrite(beerPin, HIGH);
  Serial.println("Tapping stopped: Completed");
  return true;
}

void pulseRedLed(int duration){
  digitalWrite(ledGreenPin, LOW);
  digitalWrite(ledBluePin, LOW);

  digitalWrite(ledRedPin, HIGH);
  delay(duration / 2);
  digitalWrite(ledRedPin, LOW);
  delay(duration / 2);
}
void pulseGreenLed(int duration){
  digitalWrite(ledRedPin, LOW);
  digitalWrite(ledBluePin, LOW);

  digitalWrite(ledGreenPin, HIGH);
  delay(duration / 2);
  digitalWrite(ledGreenPin, LOW);
  delay(duration / 2);
}
void pulseBlueLed(int duration){
  digitalWrite(ledRedPin, LOW);
  digitalWrite(ledGreenPin, LOW);

  digitalWrite(ledBluePin, HIGH);
  delay(duration / 2);
  digitalWrite(ledBluePin, LOW);
  delay(duration / 2);
}

void sendWssMessage(String action, String type, int user_id){
  DynamicJsonDocument wss_req(200);
  wss_req["action"] = action;
  wss_req["to"] = "server";
  wss_req["type"] = type;
  wss_req["user_id"] = user_id;

  wss_req["headers"] = JsonObject();
  wss_req["headers"]["x-auth-token"] = wss_token;

  String wss_req_str;
  serializeJson(wss_req, wss_req_str);
  wss_client.send(wss_req_str);
}