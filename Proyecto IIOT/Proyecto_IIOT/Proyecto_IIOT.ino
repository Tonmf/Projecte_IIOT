#include <ArduinoJson.h>  // https://arduinojson.org/
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient
#include <WiFi.h>
#include <Wire.h>
#include "DHT.h" // Include DHT library
#define LED_AZUL 16
#define LED_ROJO 5
#define LED_VERDE 15
#define MOTOR 13
#define PRESENCIA 17
#define DHT_PIN 21     // Defines pin number to which the sensor is connected
#define DHT_TYPE DHT11 // Defines the sensor type. It can be DHT11 or DHT22
// Define new subscription topics here
#define COMMAND_TOPIC "rele"
//#define TEST_TOPIC "P"

// Replace the next variables with your Wi-Fi SSID/Password
const char *WIFI_SSID = "AndroidAP";
const char *WIFI_PASSWORD = "plbh5028";
char macAddress[18];
bool pres=LOW;
const char *MQTT_BROKER_IP = "iiot-upc.gleeze.com";
const int MQTT_PORT = 1883;
const char *MQTT_USER = "iiot-upc";
const char *MQTT_PASSWORD = "cim2020";
const bool RETAINED = true;
const int QoS = 0; // Quality of Service for the subscriptions
float temperatura,humedad;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dhtSensor(DHT_PIN, DHT_TYPE); // Defines the sensor dht

void setup() {
  Serial.begin(9600); // Starts the serial communication
  Serial.println("\nBooting device...");
  //Sensor de temperatura
  dhtSensor.begin(); // Starts sensor communication
  //Outputs
  pinMode(LED_AZUL, OUTPUT); // Pinout as output
  pinMode(LED_VERDE, OUTPUT); // Pinout as output
  pinMode(LED_ROJO, OUTPUT); // Pinout as output
  pinMode(MOTOR, OUTPUT); // Pinout as output
  pinMode(PRESENCIA, INPUT); // Pinout as output
  
  //Comunicación MQTT
  mqttClient.setServer(MQTT_BROKER_IP,MQTT_PORT); // Connect the configured mqtt broker
  mqttClient.setCallback(callback);
  connectToWiFiNetwork(); // Connects to the configured network
  connectToMqttBroker();  // Connects to the configured mqtt broker
  setSubscriptions();     // Subscribe defined topics

   
}

void loop() {
  checkConnections(); // We check the connection every time

  // Publish every 2 seconds
  static int nowTime = millis();
  static int startTime = 0;
  static int elapsedTime = 0;
  nowTime = millis();
  elapsedTime = nowTime - startTime;
  
  sensPresencia();
  leds();
  if (elapsedTime >= 2000) {
    //publishIntNumber();   // Publishes an int number
    lecturaDHT11();
    publishDHT11();     // Publishes a big json
    startTime = nowTime;
  }
}

void leds(){

  if (pres==HIGH){
    digitalWrite(LED_VERDE,LOW);
    digitalWrite(LED_AZUL,HIGH);
    digitalWrite(LED_ROJO,LOW);
    } else{
      digitalWrite(LED_VERDE,LOW);
      digitalWrite(LED_ROJO,HIGH);
      digitalWrite(LED_AZUL,LOW);
      }
  
  }



void sensPresencia(){
  bool val;
  val = digitalRead(PRESENCIA);
   if (val == HIGH and pres == LOW)   //si está activado
   {    
        Serial.println("Sensor activado");
        pres = HIGH;
      
   } 
  }

void lecturaDHT11() {
  
  float t=dhtSensor.readTemperature();
  float h=dhtSensor.readHumidity(); 
  if(!isnan(t)){temperatura =t;}
  if(!isnan(h)){humedad = h;}
   }

void setSubscriptions() {
  subscribe(COMMAND_TOPIC);
  //subscribe(TEST_TOPIC);
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
//static const String testTopicStr = createTopic(TEST_TOPIC);

  String msg = unwrapMessage(payload, length);
  Serial.println(" => " + String(topic) + ": " + msg);

  // What to do in each topic case?
  if (String(topic) == cmdTopicStr) {
    if (msg== "true" and pres == HIGH){
      digitalWrite(MOTOR,HIGH);
      digitalWrite(LED_VERDE,HIGH);
      digitalWrite(LED_ROJO,LOW);
      digitalWrite(LED_AZUL,LOW);
      delay(2000);
      pres=LOW;
      digitalWrite(MOTOR,LOW); 
      }else if (msg=="false"){
        digitalWrite(MOTOR,LOW);
        pres=LOW;
        }
  //} else if (String(topic) == testTopicStr) {
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

/* 
void publishIntNumber() {
  static int counter = 0;
  static const String topicStr = createTopic("int_number");
  static const char *topic = topicStr.c_str();

  counter++;

  mqttClient.publish(topic, String(counter).c_str(), RETAINED);
  Serial.println(" <= " + String(topic) + ": " + String(counter));
}*/


void publishDHT11() {
  static const String topicStr = createTopic("DHT11");
  static const char *topic = topicStr.c_str();

  StaticJsonDocument<40> doc; // Create JSON document of 128 bytes
  char buffer[40];

  
  doc["T"] = temperatura; // Add names and values to the JSON document
  doc["H"] = humedad;
 
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
