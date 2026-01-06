#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

//WiFi Setup
const char* ssid = "EsDehidrasiHouse";     // WiFi ID
const char* password = "11011011";               // WiFi Password
 
//MQTT Setup
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientID = "72f71633-4a70-4988-b1bb-287722cde031"; // Client ID unik

//=== Topics ===
const char* topic_servo = "hasby/servo";     // Topik untuk kontrol servo
const char* topic_jarak = "hasby/jarak";     // Topik untuk subscribe jarak

// Setting up WiFi and MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// --- Servo Actuator Setup ---
#define SERVO_PIN 13  // Pin untuk servo
Servo myServo;

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
      
      // Subscribe ke topic servo dan jarak
      client.subscribe(topic_servo);
      Serial.print("Subscribed to: ");
      Serial.println(topic_servo);
      
      client.subscribe(topic_jarak);
      Serial.print("Subscribed to: ");
      Serial.println(topic_jarak);
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
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

  // Kontrol servo via topic_servo
  if (String(topic) == topic_servo) {
    int angle = data.toInt();
    if (angle >= 0 && angle <= 180) {
      myServo.write(angle);
      Serial.print("Servo digerakkan ke sudut: ");
      Serial.println(angle);
    } else {
      Serial.println("Sudut tidak valid! Harus antara 0-180");
    }
  }
  
  // Otomatis kontrol servo berdasarkan jarak (opsional)
  if (String(topic) == topic_jarak) {
    long jarak = data.toInt();
    Serial.print("Jarak diterima: ");
    Serial.print(jarak);
    Serial.println(" cm");
    
    // Contoh: jika jarak < 20cm, putar servo ke 90 derajat
    // jika jarak >= 20cm, kembalikan ke 0 derajat
    if (jarak < 20) {
      myServo.write(90);
      Serial.println("Servo -> 90° (objek dekat)");
    } else {
      myServo.write(0);
      Serial.println("Servo -> 0° (objek jauh)");
    }
  }
  
  Serial.println("-----------------------");
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi servo
  myServo.attach(SERVO_PIN);
  myServo.write(0); // Posisi awal
  
  // Initialize WiFi
  setup_wifi();

  // Setting MQTT server
  client.setServer(mqttServer, mqttPort); 
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}