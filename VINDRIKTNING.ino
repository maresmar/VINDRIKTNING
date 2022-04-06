#include <Wire.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <Adafruit_NeoPixel.h>
#include <SensirionI2CScd4x.h>

#include <esp_sntp.h>
#include <esp_wifi.h>
#include <soc/rtc.h>

#include "pm1006.h"
#include "env.h"

#define PIN_FAN 12
#define PIN_LED 25
#define RXD2 16
#define TXD2 17

#define BRIGHTNESS 10

#define PM_LED 1
#define TEMP_LED 2
#define CO2_LED 3

const char broker_host[] = BROKER_HOST;
const int  broker_port   = BROKER_PORT;

const char mqtt_topic_sensors[]    = MQTT_PREFIX"/sensors";
const char mqtt_topic_brightness[] = MQTT_PREFIX"/brightness";

const char hostname[]    = HOSTNAME;
const char wifi_essid[]  = WIFI_ESSID;
const char wifi_pass[]   = WIFI_PASSWORD;

static WiFiClient        wifiClient;
static MqttClient        mqttClient(wifiClient);

static PM1006            pm1006(&Serial2);
static Adafruit_NeoPixel rgbWS = Adafruit_NeoPixel(3, PIN_LED, NEO_GRB + NEO_KHZ800);
static SensirionI2CScd4x scd4x;

void setup() {
  pinMode(PIN_FAN, OUTPUT); // Fan
  rgbWS.begin(); // WS2718
  rgbWS.setBrightness(BRIGHTNESS);
  
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Wire.begin();
  uint16_t error;
  char errorMessage[256];
  scd4x.begin(Wire);
  
  Serial.println("Start...");
  Serial.println(getCpuFrequencyMhz());
  delay(500);
  Serial.println("1. LED Green");
  setColorWS(0, 255, 0, PM_LED);
  delay(1000);
  Serial.println("2. LED Green");
  setColorWS(0, 255, 0, 2);
  delay(1000);
  Serial.println("3. LED Green");
  setColorWS(0, 255, 0, 3);
  delay(1000);
  setColorWS(0, 0, 0, 1);
  setColorWS(0, 0, 0, 2);
  setColorWS(0, 0, 0, 3);

  // Start wifi
  setupWifi();

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
      Serial.print("SCD41 Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  } else {
      printSerialNumber(serial0, serial1, serial2);
  }

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      alert(CO2_LED);
  }

  Serial.println("Waiting for first measurement... (5 sec)");
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.setSleep(true);
  WiFi.onEvent(onStaGotIp, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(onStaDisconeced, SYSTEM_EVENT_STA_DISCONNECTED);

  WiFi.begin(wifi_essid, wifi_pass);
}

void setupMqtt() {
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker_host);

  if (!mqttClient.connect(broker_host, broker_port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    alert(TEMP_LED);
  }
  Serial.println("You're connected to the MQTT broker!");

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(mqtt_topic_brightness);
}

void setupSntp() {
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();

  // Set timezone to CEST
  setenv("TZ", "CEST", 1);
  tzset();
}


void onStaGotIp(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  setupMqtt();
  setupSntp();
}

void onStaDisconeced(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
}

void onMqttMessage(int messageSize) {
  // Log message details
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes.");


  if(mqttClient.messageTopic().equals(mqtt_topic_brightness)) {
    // Parse message
    uint8_t brightness = mqttClient.readString().toInt();
    
    // Log value
    Serial.print("Setting brightness to ");
    Serial.println(brightness);

    rgbWS.setBrightness(brightness);
  } else {
    Serial.println("Unsupported message");
  }
}

void loop() {
  if(mqttClient.connected()) {
    mqttClient.poll();
  }
  
  uint16_t error;
  char errorMessage[256];

  digitalWrite(PIN_FAN, HIGH);
  Serial.println("Fan ON");
  delay(10000);

  uint16_t pm2_5;
  if (pm1006.read_pm25(&pm2_5)) {
    printf("PM2.5 = %u\n", pm2_5);
  } else {
    Serial.println("Measurement failed!");
    alert(PM_LED);
  }

  delay(1000);
  digitalWrite(PIN_FAN, LOW);
  Serial.println("Fan OFF");
  
  uint16_t co2;
  float temperature;
  float humidity;
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("SCD41 Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    alert(CO2_LED);
  } else if (co2 == 0) {
    Serial.println("Invalid sample detected, skipping.");
  } else {
    //temperature = temperature -4.0;
    
    Serial.print("Co2:");
    Serial.print(co2);
    Serial.print("\t");
    Serial.print(" Temperature:");
    Serial.print(temperature);
    Serial.print("\t");
    Serial.print(" Humidity:");
    Serial.println(humidity);

    if(mqttClient.connected()) {
      // Time
      time_t now;
      time(&now);
      mqttClient.beginMessage(mqtt_topic_sensors, true);
      mqttClient.printf("{\"pm2_5\":%d,\"co2\":%d,\"temp\":%f,\"humidity\":%f,\"rssi\":%d,\"time\":%d000}",
                        pm2_5, co2, temperature, humidity, WiFi.RSSI(), now);
      mqttClient.endMessage();
    }

    if(co2 < 1000){
      setColorWS(0, 255, 0, CO2_LED);
    }
    
    if((co2 >= 1000) && (co2 < 1200)){
      setColorWS(128, 255, 0, CO2_LED);
    }
    
    if((co2 >= 1200) && (co2 < 1500)){
    setColorWS(255, 255, 0, CO2_LED);
    }
    
    if((co2 >= 1500) && (co2 < 2000)){
      setColorWS(255, 128, 0, CO2_LED);
    }
    
    if(co2 >= 2000){
      setColorWS(255, 0, 0, CO2_LED);
    }

    if(temperature < 23.0){
      setColorWS(0, 0, 255, TEMP_LED);
    }

    if((temperature >= 23.0) && (temperature < 28.0)){
      setColorWS(0, 255, 0, TEMP_LED);
    }

    if(temperature >= 28.0){
      setColorWS(255, 0, 0, TEMP_LED);
    }
  }

  // PM LED
  if(pm2_5 < 30){
    setColorWS(0, 255, 0, PM_LED);
  }
  
  if((pm2_5 >= 30) && (pm2_5 < 40)){
    setColorWS(128, 255, 0, PM_LED);
  }
  
  if((pm2_5 >= 40) && (pm2_5 < 80)){
  setColorWS(255, 255, 0, PM_LED);
  }
  
  if((pm2_5 >= 80) && (pm2_5 < 90)){
    setColorWS(255, 128, 0, PM_LED);
  }
  
  if(pm2_5 >= 90){
    setColorWS(255, 0, 0, PM_LED);
  }

  delay(60000);
}

void alert(int id){
  int i = 0;
  while (1){
     if (i > 10){
      Serial.println("Maybe need Reboot...");
      //ESP.restart();
      break;
     }
     rgbWS.setBrightness(255);
     setColorWS(255, 0, 0, id); 
     delay(200);
     rgbWS.setBrightness(BRIGHTNESS);
     setColorWS(0, 0, 0, id);
     delay(200);
     i++;
  }
}

void setColorWS(byte r, byte g, byte b, int id) {  
  uint32_t rgb;  
  rgb = rgbWS.Color(r, g, b);
  rgbWS.setPixelColor(id - 1, rgb); 
  rgbWS.show();
}

void printUint16Hex(uint16_t value) {
    Serial.print(value < 4096 ? "0" : "");
    Serial.print(value < 256 ? "0" : "");
    Serial.print(value < 16 ? "0" : "");
    Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
    Serial.print("SCD41 Serial: 0x");
    printUint16Hex(serial0);
    printUint16Hex(serial1);
    printUint16Hex(serial2);
    Serial.println();
}
