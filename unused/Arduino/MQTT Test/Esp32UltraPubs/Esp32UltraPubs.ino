#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>  // Library untuk servo di ESP32

//WiFi Setup
const char* ssid = "EsDehidrasiHouse";     // WiFi ID
const char* password = "11011011";            // WiFi Password
 
 //MQTT Setup
const char* mqttServer = "broker.hivemq.com"; // Select MQTT Server
const int mqttPort = 1883; // Select MQTT port (different port differet function)
const char* clientID = "72f71633-4a70-4988-b1bb-287722cde031"; // Client ID username (Paste it here from MQTT box or something out here)

//=== Topics ===
const char* topic_jarak = "hasby/jarak";   // Topik untuk publish jarak
// const char* mqttUserName = "bqzbdodo";
// const char* mqttPwd = "5oU2W_QN2WD8";

// Setting up WiFi and MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// --- Ultrasonic Sensor Setup ---
#define TRIG_PIN 12
#define ECHO_PIN 13

// Parameters for using non-blocking delay
unsigned long previousMillis = 0;
const long interval = 1000;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientID)) {
      Serial.println("MQTT connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);  // wait 5sec and retry
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String data = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    data += (char)payload[i];
  }
  Serial.println("");
  Serial.println("-----------------------");
}

// Fungsi untuk membaca jarak dari sensor Ultrasonik
long readDistance() {
  // Clears the trigPin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  long duration = pulseIn(ECHO_PIN, HIGH);
  
  // Calculating the distance (cm)
  // Speed of sound = 0.0343 cm/µs
  // Distance = (Time * Speed of Sound) / 2 (karena bolak-balik)
  long distance = duration * 0.0343 / 2;
  return distance;
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor dan aktuator
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Initialize device.
  setup_wifi();

  //Setting MQTT server
  client.setServer(mqttServer, mqttPort); 
  client.setCallback(callback); // Define function which will be called when a message is received.
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // (1) Read distance
    long jarak = readDistance();
    Serial.print("Distance: ");
    Serial.print(jarak);
    Serial.println(" cm");

    // (2) Kirim data jarak ke topic_jarak
    char msgJarak[8];
    ltoa(jarak, msgJarak, 10);
    client.publish(topic_jarak, msgJarak);
    Serial.println("Data published to hasby/jarak");
  } 
}