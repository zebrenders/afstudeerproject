#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include <secrets.h>

#define dht20

WiFiClient espClient;
PubSubClient client(espClient);

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
    WiFi.begin(ssid,password);
    Serial.println("Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(handleMessage);
}

void loop()
{
}
