#include <TimeLib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <DHT_U.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

WiFiServer server(80);
IPAddress local_IP(192, 168, 0, 201);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 0, 0);
const char* PUSH_SERVER = "http://192.168.0.202:8080/fan";

int PIR = 5;
int PIR_VALUE;
int PIR_FIRST_MOVEMENT_TIME = 0;
int PIR_TRIES = 0;

int TOUCH = 18;
int TOUCH_VALUE;
int TOUCH_ACTIVE = true;

int HUMIDITY = 19;
int HUMIDITY_VALUE;
float TEMPERATURE_VALUE;
DHT dht(HUMIDITY, DHT11);

int INTERNAL_LED = 2;

int FAN = 15;
int FAN_START_METHOD = 0; // 0=off; 1=manually; 2=motion; 3=remote; 4=humidity
int FAN_START_TIME;
int FAN_END_TIME;
int FAN_TIME;
int FAN_DURATION = 10; // in minutes
int FAN_TIMEOUT = 20; // in seconds

bool FAN_STATE = false;

bool MOTION_SENSOR_ACTIVE = true;
bool TOUCH_BUTON_ACTIVE = true;

String QUERY;

void blinkLed() {
  digitalWrite(INTERNAL_LED, HIGH);
  delay(250);
  digitalWrite(INTERNAL_LED, LOW);
  delay(250);
  digitalWrite(INTERNAL_LED, HIGH);
  delay(250);
  digitalWrite(INTERNAL_LED, LOW);
}

void setup() {
  Serial.begin(115200);
  
  delay(2000);
  
  Serial.println("=============================");
  Serial.println("== NEIGHBOUR POOP DETECTOR ==");
  Serial.println("=============================");
  Serial.println("Init setup...");

  pinMode(FAN, OUTPUT);
  pinMode(INTERNAL_LED, OUTPUT);
  pinMode(PIR, INPUT);
  pinMode(TOUCH, INPUT);

  blinkLed();

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());
  Serial.println();
  
  server.begin();
  dht.begin();

  blinkLed();
    
  Serial.println("Setting PIR...");
  delay(20000);

  blinkLed();
  delay(250);
  digitalWrite(INTERNAL_LED, HIGH);
  delay(2000);
  digitalWrite(INTERNAL_LED, LOW);

  Serial.println("SETUP READY!");
}

void sendStatus() {
  HTTPClient http;
  http.begin(PUSH_SERVER);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(String("{\"characteristic\":\"On\",\"value\":") + FAN_STATE + "}");
  Serial.print("Status posted on HomeKit. HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
}

void turnOn() {
  FAN_END_TIME = 0;
  FAN_STATE = true;
  FAN_START_TIME = now();
  digitalWrite(FAN, HIGH);
  digitalWrite(INTERNAL_LED, HIGH);
  delay(500);
}

void turnOff() {
  FAN_STATE = false;
  FAN_START_TIME = 0;
  FAN_START_METHOD = 0;
  FAN_END_TIME = now();
  FAN_TIME = 0;
  digitalWrite(FAN, LOW);
  digitalWrite(INTERNAL_LED, LOW);
  delay(500);
}

void loop() {
  HUMIDITY_VALUE = dht.readHumidity();
  TEMPERATURE_VALUE = dht.readTemperature();
  
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            String s = "";
            if (QUERY == "FAN_STATUS") {
              s = (FAN_STATE) ? "1" : "0";
            }
            if (QUERY == "MOTION_SENSOR") {
              s = (MOTION_SENSOR_ACTIVE) ? "1" : "0";
            }
            if (QUERY == "HUMIDITY") {
              s = HUMIDITY_VALUE;
            }
            if(QUERY == "TEMPERATURE"){
              s = TEMPERATURE_VALUE;
            }
            if(QUERY == "TOUCH_BUTTON"){
              s = (TOUCH_BUTON_ACTIVE) ? "1" : "0";;
            }
            
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(s);
            client.println();

            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        if (currentLine.endsWith("GET /fan")) {
          Serial.println("Request fan status.");
          QUERY = "FAN_STATUS";
        }
        if (currentLine.endsWith("GET /fan/H")) {
          if (FAN_STATE == true) {
            Serial.println("Fun is already turned on.");
            return;
          } else {
            Serial.println("Turn on remotly.");
            FAN_START_METHOD = 3;
            turnOn();
          }
        }
        if (currentLine.endsWith("GET /fan/L")) {
          if (FAN_STATE == false) {
            Serial.println("Fun is already turned off.");
            return;
          } else {
            Serial.println("Turn off remotly.");
            turnOff();
          }
        }
        
        if (currentLine.endsWith("GET /motion-sensor")) {
          Serial.println("Request motion sensor status.");
          QUERY = "MOTION_SENSOR";
        }
        if (currentLine.endsWith("GET /motion-sensor/H")) {
          Serial.println("Activate motion sensor.");
          MOTION_SENSOR_ACTIVE = true;
        }
        if (currentLine.endsWith("GET /motion-sensor/L")) {
          Serial.println("Deactivate motion sensor.");
          MOTION_SENSOR_ACTIVE = false;
        }

        if (currentLine.endsWith("GET /touch-button")) {
          Serial.println("Request touch button status.");
          QUERY = "TOUCH_BUTTON";
        }
        if (currentLine.endsWith("GET /touch-button/H")) {
          Serial.println("Activate touch button.");
          TOUCH_BUTON_ACTIVE = true;
        }
        if (currentLine.endsWith("GET /touch-button/L")) {
          Serial.println("Deactivate touch button.");
          TOUCH_BUTON_ACTIVE = false;
        }
                
        if (currentLine.endsWith("GET /humidity")) {
          Serial.println("Request humidity.");
          QUERY = "HUMIDITY";
        }
        if (currentLine.endsWith("GET /temperature")) {
          Serial.println("Request temperature.");
          QUERY = "TEMPERATURE";
        }
      }
    }
  } else {
    client.stop();
  }
  
  TOUCH_VALUE = digitalRead(TOUCH);
  if (TOUCH_VALUE == HIGH) {
    if(TOUCH_BUTON_ACTIVE == true){
      if (FAN_STATE == true){
        Serial.println("Turn off manually.");
        turnOff();
        sendStatus();
        delay(400);
        return;
      } else {
        Serial.println("Turn on manually.");
        FAN_START_METHOD = 1;
        turnOn();
        sendStatus();
        delay(400);
        return;
      }
    }
  }

  PIR_VALUE = digitalRead(PIR);
  if (PIR_VALUE == HIGH) {
    if (FAN_STATE == true) {
      Serial.println("Fun is already turned on.");
      return;
    }

    if(PIR_FIRST_MOVEMENT_TIME == 0) {
      PIR_FIRST_MOVEMENT_TIME = now();
      return;
    }

    int PIR_SENSIBILITY = now() - PIR_FIRST_MOVEMENT_TIME;
    if (PIR_SENSIBILITY < 5){
      return;
    }

    if (PIR_SENSIBILITY > 20){
      PIR_FIRST_MOVEMENT_TIME = now();
      return;
    }

    PIR_FIRST_MOVEMENT_TIME = 0;

    int FAN_ENDED_AGO = now() - FAN_END_TIME;
    if (FAN_ENDED_AGO < FAN_TIMEOUT) {
      Serial.println("To soon to restart, try again later.");
      return;
    };

    if (MOTION_SENSOR_ACTIVE == true) {
      Serial.print("(MOTION) Turn on automatically for ");
      Serial.print(FAN_DURATION);
      Serial.println(" minutes.");
      
      FAN_START_METHOD = 2;
      turnOn();
      sendStatus();      
      return;
    }
    
  } else {
    if (FAN_STATE == true && FAN_START_METHOD == 2) {
      FAN_TIME = now() - FAN_START_TIME;
      
      if (FAN_TIME > FAN_DURATION * 60) {
        Serial.println("(MOTION) Turn off automatically.");
        turnOff();
        sendStatus();
        return;
      } else {
        return;
      }
    }
  }
}
