#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include "HTTPClient.h"
#include <DHT.h>
#include <secrets.h>

#define DHTPIN D10
#define DHTTYPE DHT22

// test

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(DHTPIN, DHTTYPE);


// Google Apps Script URL
const String GOOGLE_APPS_SCRIPT_URL = "https://script.google.com/macros/s/";

// Keep track of number of requests
int requestCounter = 0;
int temp = 20;
int hum = 30;
int power;

void send_data_https()
{

  // Create URL with parameters to call Google Apps Script
  String urlFinal = GOOGLE_APPS_SCRIPT_URL + GOOGLE_SCRIPT_MACRO_ID + "/exec?tijdstip=" + String(requestCounter) + "&temperatuur=" + String(temp) + "&humidity=" + String(hum);

  Serial.print("POST data to spreadsheet: ");
  Serial.println(urlFinal);

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
    Serial.println("Payload: " + payload);
  }
  http.end();
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

void send_data_mqtt()
{
  power = random(0, 600);
  char value[8];
  dtostrf(power, 1, 2, value);
  client.subscribe("value");
  // client.subscribe("testTopic");
  client.publish("power", value);
}

void get_dht(){
temp = dht.readTemperature();    
hum = dht.readHumidity();

}

void setup()
{
  Serial.begin(115200);
  setup_wifi();

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
}
