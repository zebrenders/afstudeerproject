#include <Arduino.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include "HTTPClient.h"
#include <DHTesp.h>
#include <secrets.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>

#define pinDHT D3

#define RELAIS_TEMP D6
#define ATOMIZER D7
#define RELAIS_FANS D5

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
int set_time;
int set_min_temp;
int set_min_hum;
int set_max_temp;
int set_max_hum;
String ip;

// Google Apps Script URL
const String GOOGLE_APPS_SCRIPT_URL = "https://script.google.com/macros/s/";

// Keep track of number of requests
int requestCounter = 0;
int temp = 20;
int hum = 30;
int power;
char value[8];
int k = 0;

void send_data_https()
{
  requestCounter += 1;
  // Create URL with parameters to call Google Apps Script
  String urlFinal = GOOGLE_APPS_SCRIPT_URL + GOOGLE_SCRIPT_MACRO_ID + "/exec?tijdstip=" + String(requestCounter) + "&temperatuur=" + String(temp) + "&humidity=" + String(hum);

  // Serial.print("POST data to spreadsheet: ");
  // Serial.println(urlFinal);

  // Send HTTP request
  HTTPClient http;
  http.begin(urlFinal.c_str());
  // http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);

  // Get response from HTTP request
  String payload;
  if (httpCode > 0)
  {
    payload = http.getString();
    // Serial.println("Payload: " + payload);
  }
  http.end();
}

// MQTT reconnect function
void reconnect()
{
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client"))
    {
      Serial.println("connected");
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
      Serial.println(i);
      Serial.println("failed, rc=" + String(client.state()) + " retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi()
{
  delay(10);
  WiFiManager wm;
     bool res;
    res = wm.autoConnect("wifi_setup","password");

    if(!res) {
        Serial.println("Failed to connect");
    } 
    else {
        Serial.println("connected");
    }
}

void send_data_mqtt(int data)
{

  char value[8];
  dtostrf(data, 1, 2, value);
  client.subscribe("value");
  // client.subscribe("testTopic");
  client.publish("data", value);
}

void set_display(int t, int h)
{

  display.clearDisplay();

  // display temperature
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temperature: ");
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print(t);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(1);
  display.print("C");

  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Humidity: ");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print(h);
  display.print(" %");

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("progress: ");
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print(progress);
  display.print(" %");

  display.display();
}

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

void start_relais()
{
  get_dht();

  if (temp < set_min_temp)
  {
    digitalWrite(RELAIS_TEMP, HIGH);
    Serial.println("relais temp on");
  }
  else
  {
    digitalWrite(RELAIS_TEMP, LOW);
    Serial.println("relais temp off");
  }
  if (hum < set_min_hum)
  {
    digitalWrite(ATOMIZER, HIGH);
    digitalWrite(RELAIS_FANS, LOW);
    Serial.println("relais hum on");
  }
  else if (hum > set_max_hum)
  {
    digitalWrite(ATOMIZER, LOW);
    digitalWrite(RELAIS_FANS, HIGH);
    Serial.println("relais hum off");
  }
  else
  {
    digitalWrite(ATOMIZER, LOW);
    digitalWrite(RELAIS_FANS, LOW);
    Serial.println("relais hum off 2");
  }
}

void start_cycle()
{

  if (started)
  {
    intervalEnd = set_time * 60000;

    Serial.print("min temperature: ");
    Serial.println(set_min_temp);
    Serial.print("max temperature: ");
    Serial.println(set_max_temp);
    Serial.print("min humidity: ");
    Serial.println(set_min_hum);
    Serial.print("max humidity: ");
    Serial.println(set_max_hum);
    Serial.print("tijd: ");
    Serial.println(set_time);

    if (started)
    {
      unsigned long currentMillis = millis();
      previousMillisEnd = currentMillis;
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Program Started");

      display.display();
    }
    while (started == true)
    {

      // Handle MQTT connection
      if (!client.connected())
      {
        setup_wifi();
        reconnect();
      }
      client.loop();

      unsigned long currentMillis = millis();

      progress = (currentMillis - previousMillisEnd);
      Serial.println(progress);
      progress /= 60000;
      Serial.println(progress);
      progress *= 100;
      Serial.println(progress);
      progress /= set_time;
      Serial.println(progress);
      dtostrf(progress, 1, 2, value);
      client.publish("timeIn", value);

      if (currentMillis - previousMillis >= interval)
      {
        previousMillis = currentMillis;

        get_dht();
        start_relais();
      }
      if (currentMillis - previousMillisEnd >= intervalEnd)
      {
        previousMillisEnd = currentMillis;
        Serial.println("done");
        digitalWrite(ATOMIZER, LOW);
        digitalWrite(RELAIS_FANS, LOW);
        digitalWrite(RELAIS_TEMP, LOW);

        display.clearDisplay();
        display.setTextSize(3);
        display.setCursor(0, 0);
        display.print("done");
        display.setTextSize(2);
        display.setCursor(0, 10);
        display.display();
        started = false;
      }
    }
  }
}

// MQTT message handler
void handleMessage(char *topic, byte *payload, unsigned int length)
{

  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  display.clearDisplay();
  Serial.println("Received message on topic [" + String(topic) + "]: " + message);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Received message");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("on topic");
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("[" + String(topic) + "]: " + message);
  display.display();

  if (String(topic) == "temperatuur")
  {
    set_min_temp = message.toInt();
  }
  if (String(topic) == "humidity")
  {
    set_min_hum = message.toInt();
  }
  if (String(topic) == "maxHumidity")
  {
    set_max_hum = message.toInt();
  }
  if (String(topic) == "maxTemperatuur")
  {
    set_max_temp = message.toInt();
  }
  if (String(topic) == "tijd")
  {
    set_time = message.toInt();
  }
  if (String(topic) == "start")
  {
    started = message;
  }
  if (String(topic) == "stop")
  {
    started = message;
  }
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
    String i = message;
    ip = i;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("dashboard: ");
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.print(ip + ":1880/ui");
    display.display();
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();

  pinMode(ATOMIZER, OUTPUT);
  pinMode(RELAIS_TEMP, OUTPUT);
  pinMode(RELAIS_FANS, OUTPUT);

  dht.setup(pinDHT, DHTesp::DHT11);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(handleMessage);
}

void loop()
{

  // Handle MQTT connection
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  if (k < 1)
  {
    Serial.println("ip request");
    client.publish("getIp", "true");
    k += 1;
  }

  start_cycle();
}
