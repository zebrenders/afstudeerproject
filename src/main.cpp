#include <Arduino.h>
#include <WiFiManager.h>            // For automatic Wi-Fi configuration
#include <AsyncTCP.h>
#include <PubSubClient.h>           // For MQTT communication
#include "HTTPClient.h"             // For HTTP requests
#include <DHTesp.h>                 // For reading DHT sensor
#include <secrets.h>                // Contains secrets like MQTT server and script ID
#include <Adafruit_SSD1306.h>       // OLED display library
#include <Adafruit_Sensor.h>

#define pinDHT D3                   // DHT sensor pin

// Relay and actuator pins
#define RELAIS_TEMP D6
#define ATOMIZER D7
#define RELAIS_FANS D5

// OLED screen dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

unsigned long previousMillis = 0;
const unsigned long interval = 5000;
unsigned long previousMillisEnd = 0;
unsigned long intervalEnd = 0;

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

bool started = false;

float progress;
float progress2;
int set_time;
int set_min_temp;
int set_min_hum;
int set_max_temp;
int set_max_hum;
String ip;

const String GOOGLE_APPS_SCRIPT_URL = "https://script.google.com/macros/s/";

// Variables for sensor data and control
int requestCounter = 0;
int temp = 20;
int hum = 30;
int power;
char value[8];
int k = 0;

// Send temperature and humidity to Google Sheets
void send_data_https()
{
  requestCounter += 1;
  String urlFinal = GOOGLE_APPS_SCRIPT_URL + GOOGLE_SCRIPT_MACRO_ID +
                    "/exec?tijdstip=" + String(requestCounter) +
                    "&temperatuur=" + String(temp) +
                    "&humidity=" + String(hum);

  HTTPClient http;
  http.begin(urlFinal.c_str());
  int httpCode = http.GET();                 // Send HTTP GET request
  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);
  if (httpCode > 0) {
    String payload = http.getString();       // Read response payload
  }
  http.end();
}

// Connect to Wi-Fi using WiFiManager
void setup_wifi()
{
  delay(10);
  WiFiManager wm;
  bool res = wm.autoConnect("wifi_setup", "password");
  if (!res) Serial.println("Failed to connect");
  else Serial.println("connected");
}

// Reconnect to MQTT broker
void reconnect()
{
  setup_wifi();
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client"))
    {
      Serial.println("connected");
      // Subscribe to MQTT topics
      client.subscribe("testTopic");
      client.subscribe("temperatuur");
      client.subscribe("humidity");
      client.subscribe("tijd");
      client.subscribe("start");
      client.subscribe("stop");
      client.subscribe("maxTemperatuur");
      client.subscribe("maxHumidity");
      client.subscribe("ip");
    }
    else
    {
      i += 1;
      if (i > 5)
      {
        Serial.println("reconnection failed");
        break;
      }
      Serial.println("failed, retrying in 5 seconds");
      delay(5000);
    }
  }
}

// Send data through MQTT
void send_data_mqtt(int data)
{
  dtostrf(data, 1, 2, value);       // Convert float to string
  client.subscribe("value");
  client.publish("data", value);
}

// Show temperature, humidity, and progress on display
void set_display(int t, int h)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temperature: ");
  display.setCursor(0, 10);
  display.print(t);
  display.write(167);              // Degree symbol
  display.print("C");

  display.setCursor(0, 20);
  display.print("Humidity: ");
  display.setCursor(0, 30);
  display.print(h);
  display.print(" %");

  display.setCursor(0, 40);
  display.print("progress: ");
  display.setCursor(0, 50);
  display.print(progress);
  display.print(" %");

  display.display();
}

// Read sensor data, send via MQTT and HTTP, update display
void get_dht()
{
  TempAndHumidity data = dht.getTempAndHumidity();
  temp = data.temperature;
  hum = data.humidity;
  Serial.print("temperature: ");
  Serial.println(temp);
  dtostrf(temp, 1, 2, value);
  client.publish("tempIn", value);

  Serial.print("humidity: ");
  Serial.println(hum);
  dtostrf(hum, 1, 2, value);
  client.publish("humIn", value);

  set_display(temp, hum);
  send_data_https();
}

// Control relays based on temperature and humidity
void start_relais()
{
  get_dht();

  if (temp < set_min_temp) {
    digitalWrite(RELAIS_TEMP, HIGH);
  } else {
    digitalWrite(RELAIS_TEMP, LOW);
  }

  if (hum < set_min_hum) {
    digitalWrite(ATOMIZER, HIGH);
    digitalWrite(RELAIS_FANS, LOW);
  } else if (hum > set_max_hum) {
    digitalWrite(ATOMIZER, LOW);
    digitalWrite(RELAIS_FANS, HIGH);
  } else {
    digitalWrite(ATOMIZER, LOW);
    digitalWrite(RELAIS_FANS, LOW);
  }
}

// Start the environment control cycle
void start_cycle()
{
  if (started)
  {
    intervalEnd = set_time * 60000;   // Convert minutes to milliseconds
    unsigned long currentMillis = millis();
    previousMillisEnd = currentMillis;

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Program Started");
    display.display();

    while (started)
    {
      // Ensure MQTT stays connected
      if (!client.connected())
      {
        setup_wifi();
        reconnect();
      }
      client.loop();

      currentMillis = millis();
      progress = (currentMillis - previousMillisEnd) / 60000;
      progress2 = set_time - progress;
      dtostrf(progress2, 1, 2, value);
      client.publish("timeIn", value);

      progress = (progress / set_time) * 100;

      if (currentMillis - previousMillis >= interval)
      {
        previousMillis = currentMillis;
        get_dht();
        start_relais();
      }

      // End the cycle after the specified time
      if (currentMillis - previousMillisEnd >= intervalEnd)
      {
        previousMillisEnd = currentMillis;
        Serial.println("done");

        digitalWrite(ATOMIZER, LOW);
        digitalWrite(RELAIS_FANS, LOW);
        digitalWrite(RELAIS_TEMP, LOW);

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.print("done");
        display.setCursor(0, 20);
        display.print(String(mqtt_server) + ":1880/ui");
        display.display();

        started = false;
      }
    }

    // Safety: Turn off all relays at end
    digitalWrite(ATOMIZER, LOW);
    digitalWrite(RELAIS_FANS, LOW);
    digitalWrite(RELAIS_TEMP, LOW);
  }
}

// Handle incoming MQTT messages
void handleMessage(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("Received message on topic [" + String(topic) + "]: " + message);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Received message:");
  display.setCursor(0, 30);
  display.print(String(topic) + ": " + message);
  display.display();

  // Parse MQTT topic and update settings accordingly
  if (String(topic) == "temperatuur") set_min_temp = message.toInt();
  if (String(topic) == "humidity") set_min_hum = message.toInt();
  if (String(topic) == "maxHumidity") set_max_hum = message.toInt();
  if (String(topic) == "maxTemperatuur") set_max_temp = message.toInt();
  if (String(topic) == "tijd") set_time = message.toInt();
  if (String(topic) == "start") started = message == "true";
  if (String(topic) == "stop") started = false;

  if (String(topic) == "testTopic")
  {
    String i = message;
    set_min_temp = (i.substring(0, 2)).toInt();
    set_max_temp = (i.substring(2, 4)).toInt();
    set_min_hum = (i.substring(4, 6)).toInt();
    set_max_hum = (i.substring(6, 8)).toInt();
    set_time = (i.substring(8, 11)).toInt();
  }

  if (String(topic) == "ip")
  {
    ip = message;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("dashboard: ");
    display.setCursor(0, 16);
    display.print(String(mqtt_server) + ":1880/ui");
    display.display();
  }
}

// Setup function: initialize hardware and MQTT
void setup()
{
  Serial.begin(115200);
  setup_wifi();

  pinMode(ATOMIZER, OUTPUT);
  pinMode(RELAIS_TEMP, OUTPUT);
  pinMode(RELAIS_FANS, OUTPUT);

  dht.setup(pinDHT, DHTesp::DHT11);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(handleMessage);
}

// Main loop
void loop()
{
  // Ensure MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Request IP once after boot
  if (k < 1)
  {
    Serial.println("ip request");
    client.publish("getIp", "true");
    k += 1;
  }

  // Start environment cycle if triggered
  start_cycle();
}
