#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <otadrive_esp.h>

#define LEDPIN 2
#define DHT_PIN 4
#define APIKEY "4aa86850-a31b-4e76-b1a9-38334bf0e752"
#define FW_VER "v@1.0.1"

// Set wifi
const char *ssid = "AryaS";
const char *password = "lalal3214";
// Set Thingspeak
const char *mqttServer = "mqtt3.thingspeak.com";
const int mqttPort = 1883;
const char *mqttClientId = "GSM4HygJJzsTAhgqIwodIhM";
const char *mqttUsername = "GSM4HygJJzsTAhgqIwodIhM";
const char *mqttPassword = "5IfsUu/JQayWS7sVBIXKCmjJ";
long mqttChannelID = 2608008;

// Timing
const int connectionDelay = 1;
long lastPublishMillis = 0;
const long updateInterval = 15; // dalam detik

// Object
WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dhtSensor;

void konekWiFi()
{
  WiFi.mode(WIFI_STA); //Optional
    WiFi.begin(ssid, password);
    Serial.println("\nConnecting");

    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
    }
    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}

void mqttSubscriptionCallback(char *topic, byte *payload, unsigned int length)
{
    // Print detail pesan MQTT yang diterima
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];

    for (int i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    // Nyala/matikan LED berdasarkan payload MQTT
    if ((char)payload[0] == '1')
    {
        digitalWrite(LEDPIN, HIGH);
    }
    else
    {
        digitalWrite(LEDPIN, LOW);
    }
    // Parse JSON payload
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Led status ada di field3
    const char *ledState = doc["field3"];

    // Nyala matikan LED
    if (strcmp(ledState, "1") == 0)
    {
        digitalWrite(2, HIGH);
    }
    else if (strcmp(ledState, "0") == 0)
    {
        digitalWrite(2, LOW);
    }
}

// Subscribe ke channel Thingspeak
void mqttSubscribe(long subChannelID)
{
    String myTopic = "channels/" + String(subChannelID) + "/subscribe";
    client.subscribe(myTopic.c_str());
}

// Publish pesan MQTT ke ThingSpeak channel.
void mqttPublish(long pubChannelID, String message)
{
    String topicString = "channels/" + String(pubChannelID) + "/publish";
    client.publish(topicString.c_str(), message.c_str());
}

void mqttConnect()
{
    while (!client.connected())
    {
        if (client.connect(mqttClientId, mqttUsername, mqttPassword))
        {
            Serial.print("MQTT to ");
            Serial.print(mqttServer);
            Serial.print(" at port ");
            Serial.print(mqttPort);
            Serial.println(" successful.");
        }
        else
        {
            Serial.print("MQTT connection failed, rc = ");
            Serial.print(client.state());
            Serial.println(" Will try again in a few seconds");
            delay(connectionDelay * 1000);
        }
    }
}

void onUpdateProgress(int progress, int totalt);

void setup() {
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT); 
  konekWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttSubscriptionCallback);
  client.setBufferSize(2048);

  dhtSensor.setup(DHT_PIN, DHTesp::DHT11);

  OTADRIVE.setInfo(APIKEY, FW_VER);
  OTADRIVE.onUpdateFirmwareProgress(onUpdateProgress);
}

void loop() {
  // Connect ke WiFi jika terputus
    if (WiFi.status() != WL_CONNECTED)
    {
        konekWiFi();
    }
  
  // Connect if MQTT client is not connected and resubscribe to channel updates.
    if (!client.connected())
    {
        mqttConnect();
        mqttSubscribe(mqttChannelID);
    }
  
  // MQTT client loop
    client.loop();

    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    float suhu = data.temperature;
    float kelembapan = data.humidity;

    if(suhu>30)
    {
        digitalWrite(LEDPIN, HIGH);
    }
    else
    {
        digitalWrite(LEDPIN, LOW);
    }
    int ledPinStatus = digitalRead(LEDPIN);

  if (abs(long(millis()) - lastPublishMillis) > updateInterval * 1000)
    {
        lastPublishMillis = millis();

        // Skip jika data tidak valid
        if (isnan(suhu) || isnan(kelembapan))
        {
            Serial.println("Failed to read from DHT sensor!");
            return;
        }

        // Buat payload MQTT untuk di publish
        String payload = String("field1=") + String(suhu) +
                         String("&field2=") + String(kelembapan) +
                         String("&field3=") + String(ledPinStatus);

        mqttPublish(mqttChannelID, payload);
        Serial.println("Kirim Payload: " + payload);
    }

    if (OTADRIVE.timeTick(3600))
    {
      // retrive firmware info from OTAdrive server
      auto inf = OTADRIVE.updateFirmwareInfo();

      // update firmware if newer available
      if (inf.available)
      {
        log_i("\nNew version available, %dBytes, %s\n", inf.size, inf.version.c_str());
        OTADRIVE.updateFirmware();
      }
      else
      {
        log_i("\nNo newer version\n");
      }
    }
}

void onUpdateProgress(int progress, int totalt)
{
  static int last = 0;
  int progressPercent = (100 * progress) / totalt;
  Serial.print("*");
  if (last != progressPercent && progressPercent % 10 == 0)
  {
    // print every 10%
    Serial.printf("%d", progressPercent);
  }
  last = progressPercent;
}