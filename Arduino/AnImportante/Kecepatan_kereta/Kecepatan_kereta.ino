#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// KONFIGURASI WIFI & HIVEMQ
const char* ssid        = "ICT-LAB WORKSPACE";
const char* password    = "ICTLAB2024";

const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "Device02";
const char* mqtt_pass   = "Device02";

// Topic
const char* topic_speed = "smartTrain/speedometer"; 
const char* topic_pos   = "smartTrain/location";
const char* topic_gate  = "smartTrain/barrier";    // Topic Baru untuk Palang

//PIN
const int PIN_IR_1 = 13; 
const int PIN_IR_2 = 27; 
const int PIN_IR_3 = 26; 
const int PIN_IR_4 = 33; 
const int PIN_IR_5 = 35; 

//Jarak IR
const float jarak_1_ke_2 = 96.0; 
const float jarak_2_ke_3 = 80.0;
const float jarak_3_ke_4 = 97.0; 
const float jarak_4_ke_5 = 96.0; 

unsigned long t1 = 0;
unsigned long t2 = 0;
unsigned long t3 = 0;
unsigned long t4 = 0;
unsigned long t5 = 0;

int tahapan = 0; 

//Status palang (ERROR HANDLING) false = tertutup, true = terbuka
bool isPalangTertutup = false; 

WiFiClientSecure espClient;
PubSubClient client(espClient);

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

  pinMode(PIN_IR_1, INPUT_PULLUP); 
  pinMode(PIN_IR_2, INPUT_PULLUP); 
  pinMode(PIN_IR_3, INPUT_PULLUP); 
  pinMode(PIN_IR_4, INPUT_PULLUP); 
  pinMode(PIN_IR_5, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("=== SYSTEM READY (PALANG CONTROL) ===");
  Serial.println("Menunggu objek melintas...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  bool s1 = digitalRead(PIN_IR_1) == LOW;
  bool s2 = digitalRead(PIN_IR_2) == LOW;
  bool s3 = digitalRead(PIN_IR_3) == LOW;
  bool s4 = digitalRead(PIN_IR_4) == LOW;
  bool s5 = digitalRead(PIN_IR_5) == LOW;

  //Titik 1 (Start)
  if (tahapan == 0 && s1) {
    t1 = micros();
    tahapan = 1;
    Serial.println(">> Objek melewati Titik 1");

    String msgPos = "{\"titik\": \"Titik 1\"}";
    client.publish(topic_pos, msgPos.c_str());
    Serial.println("   [Topic: Posisi] " + msgPos);
  }

  //Titik 2
  else if (tahapan == 1 && s2) {
    t2 = micros();
    tahapan = 2;
    Serial.println(">> Objek melewati Titik 2");

    String msgPos = "{\"titik\": \"Titik 2\"}";
    client.publish(topic_pos, msgPos.c_str());
    Serial.println("   [Topic: Posisi] " + msgPos);

    float durasi = (float)(t2 - t1) / 1000000.0;
    float jarak  = jarak_1_ke_2;
    float v      = jarak / durasi;

    String msgSeg = "{\"tipe\": \"segmen\", \"id\": 1, \"kecepatan_s\": " + String(v, 2) + "}";
    client.publish(topic_speed, msgSeg.c_str());
    Serial.println("   [Topic: Kecepatan] " + msgSeg);
  }

  // ==========================================
  // TAHAP 3: Titik 3 (PALANG MENUTUP)
  // ==========================================
  else if (tahapan == 2 && s3) {
    t3 = micros();
    tahapan = 3;
    Serial.println(">> Objek melewati Titik 3");

    // 1. Kirim Posisi
    String msgPos = "{\"titik\": \"Titik 3\"}";
    client.publish(topic_pos, msgPos.c_str());
    Serial.println("   [Topic: Posisi] " + msgPos);

    // --- LOGIKA PALANG (ERROR HANDLING) ---
    if (isPalangTertutup == false) {
      // Kondisi Normal: Palang masih terbuka, sekarang kita tutup
      String msgGate = "{\"status\": \"Tertutup\"}";
      client.publish(topic_gate, msgGate.c_str());
      
      isPalangTertutup = true; // Update status menjadi tertutup
      Serial.println("   [Topic: Palang] " + msgGate);
    } 
    else {
      // Kondisi Error: Palang sudah tertutup sebelum sampai titik 3
      Serial.println("   [WARNING] Palang sudah tertutup sebelumnya! Perintah diabaikan.");
    }
    // --------------------------------------

    // 2. Kirim Kecepatan
    float durasi = (float)(t3 - t2) / 1000000.0;
    float jarak  = jarak_2_ke_3;
    float v      = jarak / durasi;

    String msgSeg = "{\"tipe\": \"segmen\", \"id\": 2, \"kecepatan_s\": " + String(v, 2) + "}";
    client.publish(topic_speed, msgSeg.c_str());
    Serial.println("   [Topic: Kecepatan] " + msgSeg);
  }

  // ==========================================
  // TAHAP 4: Titik 4
  // ==========================================
  else if (tahapan == 3 && s4) {
    t4 = micros();
    tahapan = 4;
    Serial.println(">> Objek melewati Titik 4");

    String msgPos = "{\"titik\": \"Titik 4\"}";
    client.publish(topic_pos, msgPos.c_str());
    Serial.println("   [Topic: Posisi] " + msgPos);

    float durasi = (float)(t4 - t3) / 1000000.0;
    float jarak  = jarak_3_ke_4;
    float v      = jarak / durasi;

    String msgSeg = "{\"tipe\": \"segmen\", \"id\": 3, \"kecepatan_s\": " + String(v, 2) + "}";
    client.publish(topic_speed, msgSeg.c_str());
    Serial.println("   [Topic: Kecepatan] " + msgSeg);
  }

  // ==========================================
  // TAHAP 5: Titik 5 (Finish & PALANG MEMBUKA)
  // ==========================================
  else if (tahapan == 4 && s5) {
    t5 = micros();
    tahapan = 0;

    Serial.println(">> Objek melewati Titik 5");

    // 1. Kirim Posisi
    String msgPos = "{\"titik\": \"Titik 5\"}";
    client.publish(topic_pos, msgPos.c_str());
    Serial.println("   [Topic: Posisi] " + msgPos);

    // --- LOGIKA PALANG (MEMBUKA KEMBALI) ---
    // Di titik akhir, kita paksa buka (reset) apapun status sebelumnya
    String msgGate = "{\"status\": \"Terbuka\"}";
    client.publish(topic_gate, msgGate.c_str());
    
    isPalangTertutup = false; // Reset status menjadi terbuka untuk kereta berikutnya
    Serial.println("   [Topic: Palang] " + msgGate);
    // ---------------------------------------

    // 2. Kirim Kecepatan Segmen 4
    float durasi_seg4 = (float)(t5 - t4) / 1000000.0;
    float jarak_seg4  = jarak_4_ke_5;
    float v4          = jarak_seg4 / durasi_seg4;

    String msgSeg = "{\"tipe\": \"segmen\", \"id\": 4, \"kecepatan_s\": " + String(v4, 2) + "}";
    client.publish(topic_speed, msgSeg.c_str());
    Serial.println("   [Topic: Kecepatan] " + msgSeg);

    // 3. Kirim Rata-rata
    float total_waktu = (float)(t5 - t1) / 1000000.0;
    float total_jarak = jarak_1_ke_2 + jarak_2_ke_3 + jarak_3_ke_4 + jarak_4_ke_5; 
    float v_avg       = total_jarak / total_waktu;

    String msgAvg = "{\"tipe\": \"rata_rata\", \"kecepatan_r\": " + String(v_avg, 2) +
                    ", \"waktu_total\": " + String(total_waktu, 4) + "}";
    client.publish(topic_speed, msgAvg.c_str());
    Serial.println("   [Topic: Kecepatan] " + msgAvg);

    Serial.println("------------------------------\n");
    delay(2000);
    Serial.println("Siap untuk objek berikutnya...");
  }
}