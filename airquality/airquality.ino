#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "ccs811.h"

CCS811 ccs811(23);

const char* ssid = "";
const char* pass = "";
const char* mqtt_broker = "192.168.178.20";

WiFiClient esp_client;
PubSubClient client(esp_client);
double wifi_timeout = 10000;
int max_reconnect_trys = 10;
const char* mqtt_topic = "home-assistant/andre/air_quality";
const char* co2_topic = "home-assistant/andre/air_quality/co2";
const char* tvoc_topic = "home-assistant/andre/air_quality/tvoc";

void setup() {
    Serial.begin(115200);
    Serial.println("Starting CCS811 air quality measurement");
    Serial.print("library version: ");
    Serial.println(CCS811_VERSION);

    Wire.begin();
    ccs811.set_i2cdelay(50);
    bool ok = ccs811.begin();
    if(!ok) Serial.println("setup of CCS811 FAILED");

    Serial.print("hardware    version:");
    Serial.println(ccs811.hardware_version(), HEX);
    Serial.print("bootloader  version:");
    Serial.println(ccs811.bootloader_version(), HEX);
    Serial.print("application version:");
    Serial.println(ccs811.application_version(), HEX);

    ok = ccs811.start(CCS811_MODE_1SEC);
    if(!ok) Serial.println("CCS811 start FAILED");
    setup_wifi();
    setup_mqtt();

}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  int connection_trys = 1;
  while(true) {
    if(WiFi.status() == WL_CONNECTED) {
      break;
    }
    if (connection_trys > max_reconnect_trys) {
      Serial.println("Max reconnect attempts reached");
      return;
    }
    double start_wifi_time = millis();
    WiFi.begin(ssid, pass);
    while(WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if((millis() - start_wifi_time) > wifi_timeout) {
        connection_trys++;
        Serial.println("retry connect " + String(connection_trys));
        break;
      }
    }
  }
  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println("IP addr.: ");
  Serial.print(WiFi.localIP());
  Serial.println("");
}

void setup_mqtt() {
  client.setServer(mqtt_broker, 1883);
  if(client.connect("ESP32_air_quality_sensor")) {
    Serial.println("Connected to MQTT Broker");
  }
  else {
    Serial.println("Can't connect to MQTT Broker :(");
  }
}

void reconnect() {
    while(!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32_air_quality_sensor")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void loop() {
    if(!client.connected()) {
        reconnect();
    }
    client.loop();
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);

    if(errstat==CCS811_ERRSTAT_OK) {
        Serial.print("CCS811: ");
        Serial.print("eco2=");  Serial.print(eco2);     Serial.print(" ppm  ");
        Serial.print("etvoc="); Serial.print(etvoc);    Serial.print(" ppb  ");
        Serial.println();

        String s;
        s = (String)eco2;
        char buf1[sizeof(s)];
        s.toCharArray(buf1, sizeof(s));
        client.publish(co2_topic, buf1);

        s = (String)etvoc;
        char buf2[sizeof(s)];
        s.toCharArray(buf2, sizeof(s));
        client.publish(tvoc_topic, buf2);

        // String msg = String(eco2);
        // char __buffer[sizeof(msg)];
        // msg.toCharArray(__buffer, sizeof(msg));
        // client.publish(co2_topic, __buffer);

        // msg = String(etvoc)

        // // char* co2_msg;
        // // char* tvoc_msg;
        // // co2_msg = (char*)&eco2;
        // // tvoc_msg = (char*)&etvoc;
        // client.publish(tvoc_topic, (char*)etvoc);
    } else if( errstat==CCS811_ERRSTAT_OK_NODATA ) {
        Serial.println("CCS811: waiting for (new) data");
    } else if( errstat & CCS811_ERRSTAT_I2CFAIL ) { 
        Serial.println("CCS811: I2C error");
    } else {
        Serial.print("CCS811: errstat="); Serial.print(errstat,HEX); 
        Serial.print("="); Serial.println( ccs811.errstat_str(errstat) ); 
    }
    delay(1000);
}
