#include <ArduinoJson.h>  // https://arduinojson.org/
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient
#include <WiFi.h>
#include <Wire.h>
#include "DHT.h" // Include DHT library

// Define DHT sensor
#define DHT_PIN 2     // Defines pin number to which the sensor is connected
#define DHT_TYPE DHT11 // Defines the sensor type. It can be DHT11 or DHT22

// Define new subscription topics here
#define COMMAND_TOPIC "cmd"
#define TEST_TOPIC "test"

DHT dhtSensor(DHT_PIN, DHT_TYPE); // Defines the sensor dht
float temperature; // Global variable
float humidity; // Global variable

const char *pulsador;

// Replace the next variables with your Wi-Fi SSID/Password
const char *WIFI_SSID = "MIWIFI_2G_mg7m";
const char *WIFI_PASSWORD = "jR3dhv6j";
char macAddress[18];

// Add MQTT Broker settings
const char *MQTT_BROKER_IP = "iiot-upc.gleeze.com";
const int MQTT_PORT = 1883;
const char *MQTT_USER = "iiot-upc";
const char *MQTT_PASSWORD = "cim2020";
const bool RETAINED = true;
const int QoS = 0; // Quality of Service for the subscriptions


WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() {
  Serial.begin(9600); // Starts the serial communication
  Serial.println("\nBooting device...");

  dhtSensor.begin(); // Starts sensor communication
  pinMode(25, OUTPUT);
  pinMode(15, INPUT);
  
  mqttClient.setServer(MQTT_BROKER_IP,
                       MQTT_PORT); // Connect the configured mqtt broker

  mqttClient.setCallback(
      callback); // Prepare what to do when a message is recieved

  connectToWiFiNetwork(); // Connects to the configured network
  connectToMqttBroker();  // Connects to the configured mqtt broker
  setSubscriptions();     // Subscribe defined topics
}

void loop() {

  updatetemperature();
  updatehumidity();
  updatepulsador();
  
  checkConnections(); // We check the connection every time

  // Publish every 2 seconds
  static int nowTime = millis();
  static int startTime = 0;
  static int elapsedTime = 0;
  nowTime = millis();
  elapsedTime = nowTime - startTime;
  if (elapsedTime >= 2000) {
    publishTemperature();// Publica temperatura
    publishHumidity();// Publica humedad
    publishPuerta();// Publica pulsador puerta
    publishDatos();
    startTime = nowTime;
  }  
delay(2000);
}

/* Additional functions */
// Pulsador puerta
void updatepulsador(){
  /*
  pulsador = digitalRead(15);
  Serial.println("Updated pulsador: " + String(pulsador) + ""); // Prints in a new line the result
  */
   if (digitalRead(15) == LOW) {
        pulsador = "false";
        }
        else{
          pulsador = "true";  
        }
        Serial.println("Updated pulsador: " + String(pulsador) + ""); // Prints in a new line the result
}

// DHT temperature update
void updatetemperature() {
  float t = dhtSensor.readTemperature(); // Updates temperature
  if(!isnan(t)){
    temperature = t; // Updates the value to global variable
    Serial.println("Updated temperature: " + String(temperature) + "°C"); // Prints in a new line the result
  } else {
    Serial.println("Sensor had a bad reading");
  }
}
// DHT humidity update
void updatehumidity() {
  float h = dhtSensor.readHumidity(); // Updates temperature
  if(!isnan(h)){
    humidity = h; // Updates the value to global variable
    Serial.println("Updated humidity: " + String(humidity) + "%"); // Prints in a new line the result
  } else {
    Serial.println("Sensor had a bad reading");
  }
}

// Topic Temperatura
void publishTemperature() {
  static const String topicStr = createTopic("T");
  static const char *topic = topicStr.c_str();

  mqttClient.publish(topic, String(temperature).c_str(), RETAINED);
  Serial.println(" <= " + String(topic) + ": " + String(temperature));
}

// Topic Humedad
void publishHumidity() {
  static const String topicStr = createTopic("H");
  static const char *topic = topicStr.c_str();

  mqttClient.publish(topic, String(humidity).c_str(), RETAINED);
  Serial.println(" <= " + String(topic) + ": " + String(humidity));
}

// Topic pulsador puerta
void publishPuerta() {
  static const String topicStr = createTopic("P");
  static const char *topic = topicStr.c_str();

  String text = String(pulsador);
  mqttClient.publish(topic, text.c_str(), RETAINED);
  Serial.println(" <= " + String(topic) + ": " + text);

  /*mqttClient.publish(topic, String(pulsador).c_str(), RETAINED);
  Serial.println(" <= " + String(topic) + ": " + String(pulsador));*/
}
// Datos del DHT en JSON
void publishDatos() {
  static const String topicStr = createTopic("Datos");
  static const char *topic = topicStr.c_str();

  StaticJsonDocument<128> doc; // Create JSON document of 128 bytes
  char buffer[128]; // Create the buffer where we will print the JSON document
                    // to publish through MQTT
  doc["T"] = temperature;
  doc["H"] = humidity;
  doc["P"] = pulsador;

  // Serialize the JSON document to a buffer in order to publish it
  serializeJson(doc, buffer);
  mqttClient.publish(topic, buffer, RETAINED);
  Serial.println(" <= " + String(topic) + ": " + String(buffer));
}
String createTopic(char *topic) {
  String topicStr = String(macAddress) + "/" + topic;
  return topicStr;
}

void connectToWiFiNetwork() {
  Serial.print(
      "Connecting with Wi-Fi: " +
      String(WIFI_SSID)); // Print the network which you want to connect
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".."); // Connecting effect
  }
  Serial.print("..connected!  (ip: "); // After being connected to a network,
                                       // our ESP32 should have a IP
  Serial.print(WiFi.localIP());
  Serial.println(")");
  String macAddressStr = WiFi.macAddress().c_str();
  strcpy(macAddress, macAddressStr.c_str());
}

void connectToMqttBroker() {
  Serial.print(
      "Connecting with MQTT Broker:" +
      String(MQTT_BROKER_IP));    // Print the broker which you want to connect
  mqttClient.connect(macAddress, MQTT_USER, MQTT_PASSWORD);// Using unique mac address from ESP32
  while (!mqttClient.connected()) {
    delay(500);
    Serial.print("..");             // Connecting effect
    mqttClient.connect(macAddress); // Using unique mac address from ESP32
  }
  Serial.println("..connected! (ClientID: " + String(macAddress) + ")");
}

void checkConnections() {
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else { // Try to reconnect
    Serial.println("Connection has been lost with MQTT Broker");
    if (WiFi.status() != WL_CONNECTED) { // Check wifi connection
      Serial.println("Connection has been lost with Wi-Fi");
      connectToWiFiNetwork(); // Reconnect Wifi
    }
    connectToMqttBroker(); // Reconnect Server MQTT Broker
  }
}

// MQTT subscribe section

void setSubscriptions() {
  subscribe(COMMAND_TOPIC);
  subscribe(TEST_TOPIC);
}

void subscribe(char *newTopic) {
  const String topicStr = createTopic(newTopic);
  const char *topic = topicStr.c_str();
  mqttClient.subscribe(topic, QoS);
  Serial.println("Client MQTT subscribed to topic: " + topicStr +
                 " (QoS:" + String(QoS) + ")");
}

void callback(char *topic, byte *payload, unsigned int length) {
  // Register all subscription topics
  static const String cmdTopicStr = createTopic(COMMAND_TOPIC);
  static const String testTopicStr = createTopic(TEST_TOPIC);

  String msg = unwrapMessage(payload, length);
  Serial.println(" => " + String(topic) + ": " + msg);

  // What to do in each topic case?
  if (String(topic) == cmdTopicStr) {
    // Do some command
      if (msg== "1"){
        digitalWrite(25, HIGH);
      }
      else{
        digitalWrite(25, LOW);   
      }
  } else if (String(topic) == testTopicStr) {
    // Do some other stuff
  } else {
    Serial.println("[WARN] - '" + String(topic) +
                   "' topic was correctly subscribed but not defined in the "
                   "callback function");
  }
}


String unwrapMessage(byte *message, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) { // Unwraps the string message
    msg += (char)message[i];
  }
  return msg;
}
