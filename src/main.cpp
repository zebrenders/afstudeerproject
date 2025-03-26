#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include "HTTPClient.h"
#include <DHT.h>
#include <secrets.h>

#define DHTPIN D10
#define DHTTYPE DHT22
#define RELAIS_TEMP D3
#define RELAIS_HUM D4
#define RELAIS_FANS D6

unsigned long previousMillis = 0;
const unsigned long interval = 5000;
unsigned long previousMillisEnd = 0;
unsigned long intervalEnd = 0;

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(DHTPIN, DHTTYPE);

bool started = false;

int set_time;
int set_min_temp;
int set_min_hum;
int set_max_temp;
int set_max_hum;

// Google Apps Script URL
const String GOOGLE_APPS_SCRIPT_URL = "https://script.google.com/macros/s/";

// Keep track of number of requests
int requestCounter = 0;
int temp = 20;
int hum = 30;
int power;

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
    }
    else
    {
      Serial.println("failed, rc=" + String(client.state()) + " retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void send_data_mqtt(int data)
{

  char value[8];
  dtostrf(data, 1, 2, value);
  client.subscribe("value");
  // client.subscribe("testTopic");
  client.publish("data", value);
}

void get_dht()
{
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  Serial.print("temperature: ");
  Serial.println(temp);

  Serial.print("humidity: ");
  Serial.println(hum);

  temp = 20;
  hum = 30;

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
    digitalWrite(RELAIS_HUM, HIGH);
    digitalWrite(RELAIS_FANS, LOW);
    Serial.println("relais hum on");
  }

  else if (hum > set_max_hum)
  {
    digitalWrite(RELAIS_HUM, LOW);
    digitalWrite(RELAIS_FANS, HIGH);
    Serial.println("relais hum off");
  }
  else
  {
    digitalWrite(RELAIS_HUM, LOW);
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

    while (started)
    {

      // Handle MQTT connection
      if (!client.connected())
      {
        reconnect();
      }
      client.loop();

      unsigned long currentMillis = millis();

      if (currentMillis - previousMillis >= interval)
      {
        previousMillis = currentMillis;

        get_dht();
        start_relais();
      }
      if (currentMillis - previousMillisEnd >= intervalEnd)
      {
        previousMillisEnd = currentMillis;
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
  Serial.println("Received message on topic [" + String(topic) + "]: " + message);

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
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();

  pinMode(RELAIS_HUM, OUTPUT);
  pinMode(RELAIS_TEMP, OUTPUT);
  pinMode(RELAIS_FANS, OUTPUT);

  dht.begin();

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

  start_cycle();
}
