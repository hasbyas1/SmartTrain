#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>     
#include "time.h"

const char* ssid       = "ICT-LAB WORKSPACE";
const char* password   = "ICTLAB2024";

const char* mqtt_server = "737108d634bf4569a5322be25537a876.s1.eu.hivemq.cloud"; 
const int   mqtt_port   = 8883;
const char* mqtt_user   = "Device02";
const char* mqtt_pass   = "Device02";
const char* mqtt_topic  = "esp32/rfid";

//KONFIGURASI WAKTU
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;  // GMT+7
const int   daylightOffset_sec = 0;

#define RST_PIN  6
#define SS_PIN   10 
#define SCK_PIN  9
#define MISO_PIN 7
#define MOSI_PIN 8

MFRC522 mfrc522(SS_PIN, RST_PIN); 

WiFiClientSecure espClient;
PubSubClient client(espClient);

// Fungsi mendapatkan String Waktu
String getTimeString() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Waktu Error";
  }
  char timeStringBuff[50]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Terhubung!");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  Serial.printf("Menghubungkan ke %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" TERHUBUNG!");

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sinkronisasi waktu...");
  
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init(); 
  
  Serial.println(F("-------------------------"));
  Serial.println(F("   SISTEM ABSENSI SIAP   "));
  Serial.println(F("-------------------------"));
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if ( ! mfrc522.PICC_IsNewCardPresent()) { return; }
  if ( ! mfrc522.PICC_ReadCardSerial()) { return; }

  Serial.print(F("UID Tag :"));
  String content= "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  String uidClean = content.substring(1);
  String waktuSekarang = getTimeString();

  String mqttPayload = "";

  // --- LOGIKA UTAMA ---
  if (uidClean == "73 73 CB 0C") { 
    Serial.println("Akses : DITERIMA");
    Serial.println("Waktu : " + waktuSekarang);
    
    // Format JSON KHUSUS DITERIMA
    // {"uid":"XX", "akses":"DITERIMA", "waktu":"XX", "status":"Kereta Berangkat"}
    mqttPayload = "{\"uid\":\"" + uidClean + "\", \"akses\":\"DITERIMA\", \"waktu\":\"" + waktuSekarang + "\", \"status\":\"Kereta Berangkat\"}";
    
    // PUBLISH KE MQTT
    client.publish(mqtt_topic, mqttPayload.c_str());
    Serial.println("Payload: " + mqttPayload);
    Serial.println("Data terkirim ke MQTT!");
    
    delay(2000); 
  } else {
    Serial.println("Akses : DITOLAK");
    
    // Format JSON KHUSUS DITOLAK
    // {"uid":"XX", "akses":"DITOLAK"}
    mqttPayload = "{\"uid\":\"" + uidClean + "\", \"akses\":\"DITOLAK\"}";
    
    client.publish(mqtt_topic, mqttPayload.c_str());
    Serial.println("Payload: " + mqttPayload);
    Serial.println("Data terkirim ke MQTT!");
    
    delay(1000);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}