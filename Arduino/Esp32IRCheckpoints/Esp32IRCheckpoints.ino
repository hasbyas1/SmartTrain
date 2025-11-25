#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ==================================================
// KONFIGURASI WIFI & HIVEMQ
// ==================================================
const char* ssid        = "EsDehidrasiHouse";
const char* password    = "11011011";

// Detail dari Dashboard HiveMQ Cloud
const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "Device02";
const char* mqtt_pass   = "Device02";

const char* mqtt_topic  = "esp32/kecepatan";

// ==================================================
// DEFINISI PIN (Sesuai Code Terakhir)
// ==================================================
const int PIN_IR_1 = 13; // Titik 1 (Start)
const int PIN_IR_2 = 27; // Titik 2
const int PIN_IR_3 = 26; // Titik 3
const int PIN_IR_4 = 33; // Titik 4
const int PIN_IR_5 = 35; // Titik 5 (Finish - Input Only)

// ==================================================
// KONFIGURASI JARAK (CM)
// ==================================================
const float jarak_1_ke_2 = 20.0; 
const float jarak_2_ke_3 = 20.0;
const float jarak_3_ke_4 = 20.0; 
const float jarak_4_ke_5 = 20.0; 

// Variabel Waktu
unsigned long t1 = 0;
unsigned long t2 = 0;
unsigned long t3 = 0;
unsigned long t4 = 0;
unsigned long t5 = 0;

// Tahapan: 0=Siap, 1=Lewat S1, 2=Lewat S2, 3=Lewat S3, 4=Lewat S4
int tahapan = 0; 

// Objek MQTT
WiFiClientSecure espClient;
PubSubClient client(espClient);

// --- Setup WiFi ---
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Terhubung");
  
  espClient.setInsecure(); 
}

// --- Reconnect MQTT ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT HiveMQ...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("berhasil!");
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Setup Pin
  pinMode(PIN_IR_1, INPUT_PULLUP); 
  pinMode(PIN_IR_2, INPUT_PULLUP); 
  pinMode(PIN_IR_3, INPUT_PULLUP); 
  pinMode(PIN_IR_4, INPUT_PULLUP); 
  pinMode(PIN_IR_5, INPUT); // Pin 35 Input Only 

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("=== SYSTEM READY (JSON UPDATED) ===");
  Serial.println("Menunggu objek melintas...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Baca semua sensor
  bool s1 = digitalRead(PIN_IR_1) == LOW;
  bool s2 = digitalRead(PIN_IR_2) == LOW;
  bool s3 = digitalRead(PIN_IR_3) == LOW;
  bool s4 = digitalRead(PIN_IR_4) == LOW;
  bool s5 = digitalRead(PIN_IR_5) == LOW;

  // ==========================================
  // TAHAP 1: Start (Titik 1)
  // ==========================================
  if (tahapan == 0 && s1) {
    t1 = micros();
    tahapan = 1;
    Serial.println(">> Objek melewati Titik 1");
  }

  // ==========================================
  // TAHAP 2: Checkpoint (Titik 2)
  // ==========================================
  else if (tahapan == 1 && s2) {
    t2 = micros();
    tahapan = 2;
    Serial.println(">> Objek melewati Titik 2");

    float durasi = (float)(t2 - t1) / 1000000.0;
    float jarak  = jarak_1_ke_2 / 100.0;
    float v      = jarak / durasi;

    // --- KIRIM JSON SEGMEN 1 (UPDATED) ---
    // Format: {"tipe": "segmen", "id": 1, "kecepatan": 0.xxx}
    String msg = "{\"tipe\": \"segmen\", \"id\": 1, \"kecepatan_s\": " + String(v, 3) + "}";
    client.publish(mqtt_topic, msg.c_str());
    Serial.println("   JSON Sent: " + msg);
  }

  // ==========================================
  // TAHAP 3: Checkpoint (Titik 3)
  // ==========================================
  else if (tahapan == 2 && s3) {
    t3 = micros();
    tahapan = 3;
    Serial.println(">> Objek melewati Titik 3");

    float durasi = (float)(t3 - t2) / 1000000.0;
    float jarak  = jarak_2_ke_3 / 100.0;
    float v      = jarak / durasi;

    // --- KIRIM JSON SEGMEN 2 (UPDATED) ---
    String msg = "{\"tipe\": \"segmen\", \"id\": 2, \"kecepatan_s\": " + String(v, 3) + "}";
    client.publish(mqtt_topic, msg.c_str());
    Serial.println("   JSON Sent: " + msg);
  }

  // ==========================================
  // TAHAP 4: Checkpoint (Titik 4)
  // ==========================================
  else if (tahapan == 3 && s4) {
    t4 = micros();
    tahapan = 4;
    Serial.println(">> Objek melewati Titik 4");

    float durasi = (float)(t4 - t3) / 1000000.0;
    float jarak  = jarak_3_ke_4 / 100.0;
    float v      = jarak / durasi;

    // --- KIRIM JSON SEGMEN 3 (UPDATED) ---
    String msg = "{\"tipe\": \"segmen\", \"id\": 3, \"kecepatan_s\": " + String(v, 3) + "}";
    client.publish(mqtt_topic, msg.c_str());
    Serial.println("   JSON Sent: " + msg);
  }

  // ==========================================
  // TAHAP 5: Finish (Titik 5)
  // ==========================================
  else if (tahapan == 4 && s5) {
    t5 = micros();
    tahapan = 0; // Reset
    Serial.println(">> Objek melewati Titik 5");

    // Hitung V4 (4 ke 5)
    float durasi_seg4 = (float)(t5 - t4) / 1000000.0;
    float jarak_seg4  = jarak_4_ke_5 / 100.0;
    float v4          = jarak_seg4 / durasi_seg4;

    // --- KIRIM JSON SEGMEN 4 (UPDATED) ---
    String msgV4 = "{\"tipe\": \"segmen\", \"id\": 4, \"kecepatan_s\": " + String(v4, 3) + "}";
    client.publish(mqtt_topic, msgV4.c_str());
    Serial.println("   JSON Sent: " + msgV4);

    // --- HITUNG RATA-RATA TOTAL ---
    float total_waktu = (float)(t5 - t1) / 1000000.0;
    float total_jarak = (jarak_1_ke_2 + jarak_2_ke_3 + jarak_3_ke_4 + jarak_4_ke_5) / 100.0;
    float v_avg       = total_jarak / total_waktu;

    // --- KIRIM JSON RATA-RATA (Tetap konsisten dengan tipe) ---
    // Format: {"tipe": "rata_rata", "kecepatan": 0.xxx, "waktu_total": x.xxx}
    String msgAvg = "{\"tipe\": \"rata_rata\", \"kecepatan_r\": " + String(v_avg, 3) + ", \"waktu_total\": " + String(total_waktu, 4) + "}";
    client.publish(mqtt_topic, msgAvg.c_str());

    // Output Serial Akhir
    Serial.println("   JSON Sent: " + msgAvg);
    Serial.println("------------------------------\n");

    delay(2000);
    Serial.println("Siap untuk objek berikutnya...");
  }
}