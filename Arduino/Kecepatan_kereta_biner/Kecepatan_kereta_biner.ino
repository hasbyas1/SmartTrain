#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// 4 segmen kecepatan (1-2, 2-3, 3-4, 4-5)
#define BATCH_SIZE 4 

struct TrainTelemetryFrame {
  float start_timestamp;         // Float 4 bytes
  float data_kecepatan[BATCH_SIZE]; // Array 4 data
                                    // pakai float agar presisi
                                    // pakai int16_t untuk hemat size.
};

TrainTelemetryFrame myPacket;
int sample_index = 0; // Untuk menghitung data ke berapa yang sedang diisi

const char* ssid        = "ICT-LAB WORKSPACE";
const char* password    = "ICTLAB2024";

const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "Device02";
const char* mqtt_pass   = "Device02";

// TOPIC
const char* topic_batch = "smartTrain/telemetry_batch"; // Data dikirim sekaligus di sini
const char* topic_gate  = "smartTrain/barrier";       // Palang tetap real-time

// PIN
const int PIN_IR_1 = 13; 
const int PIN_IR_2 = 27; 
const int PIN_IR_3 = 26; 
const int PIN_IR_4 = 33; 
const int PIN_IR_5 = 35; 

// JARAK (CM)
const float jarak_1_ke_2 = 96.0; 
const float jarak_2_ke_3 = 80.0;
const float jarak_3_ke_4 = 97.0; 
const float jarak_4_ke_5 = 96.0; 

// WAKTU
unsigned long t1, t2, t3, t4, t5;

// STATUS
int tahapan = 0; 
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
  Serial.println("\nWiFi Terhubung");
  espClient.setInsecure(); 
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT HiveMQ...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("berhasil!");
    } else {
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

  Serial.println("=== SYSTEM READY (BATCH MODE) ===");
  Serial.println("Data kecepatan akan dikumpulkan dan dikirim di akhir (Titik 5).");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  bool s1 = digitalRead(PIN_IR_1) == LOW;
  bool s2 = digitalRead(PIN_IR_2) == LOW;
  bool s3 = digitalRead(PIN_IR_3) == LOW;
  bool s4 = digitalRead(PIN_IR_4) == LOW;
  bool s5 = digitalRead(PIN_IR_5) == LOW;

  // ==========================================
  // TAHAP 1: START & INISIALISASI BATCH
  // ==========================================
  if (tahapan == 0 && s1) {
    t1 = micros();
    tahapan = 1;
    
    // Reset data struct
    sample_index = 0; 
    myPacket.start_timestamp = (float)millis() / 1000.0; // Catat waktu mulai (detik)
    
    Serial.println(">> Titik 1: Mulai merekam batch...");
  }

  // ==========================================
  // TAHAP 2: HITUNG SPEED & SIMPAN KE BUFFER
  // ==========================================
  else if (tahapan == 1 && s2) {
    t2 = micros();
    tahapan = 2;
    Serial.println(">> Titik 2");

    // Hitung
    float v = jarak_1_ke_2 / ((float)(t2 - t1) / 1000000.0);
    
    // SIMPAN KE STRUCT (Belum dikirim)
    if(sample_index < BATCH_SIZE) {
        myPacket.data_kecepatan[sample_index] = v;
        sample_index++; 
        Serial.printf("   [Buffer] Data 1 disimpan: %.2f cm/s\n", v);
    }
  }

  // ==========================================
  // TAHAP 3: BUFFER DATA + LOGIKA PALANG (REALTIME)
  // ==========================================
  else if (tahapan == 2 && s3) {
    t3 = micros();
    tahapan = 3;
    Serial.println(">> Titik 3");

    // 1. LOGIKA PALANG (Harus Realtime, tidak boleh di-batch)
    if (!isPalangTertutup) {
      String msgGate = "{\"status\": \"Tertutup\"}";
      // Kirim status palang langsung
      client.publish(topic_gate, msgGate.c_str()); 
      isPalangTertutup = true;
      Serial.println("   [Palang] Perintah TUTUP dikirim.");
    }

    // 2. SIMPAN DATA KECEPATAN KE BUFFER
    float v = jarak_2_ke_3 / ((float)(t3 - t2) / 1000000.0);
    if(sample_index < BATCH_SIZE) {
        myPacket.data_kecepatan[sample_index] = v;
        sample_index++;
        Serial.printf("   [Buffer] Data 2 disimpan: %.2f cm/s\n", v);
    }
  }

  // ==========================================
  // TAHAP 4: SIMPAN KE BUFFER
  // ==========================================
  else if (tahapan == 3 && s4) {
    t4 = micros();
    tahapan = 4;
    Serial.println(">> Titik 4");

    float v = jarak_3_ke_4 / ((float)(t4 - t3) / 1000000.0);
    if(sample_index < BATCH_SIZE) {
        myPacket.data_kecepatan[sample_index] = v;
        sample_index++;
        Serial.printf("   [Buffer] Data 3 disimpan: %.2f cm/s\n", v);
    }
  }

  // ==========================================
  // TAHAP 5: FINISH & KIRIM BATCH (PUBLISH)
  // ==========================================
  else if (tahapan == 4 && s5) {
    t5 = micros();
    tahapan = 0;
    Serial.println(">> Titik 5: Finish!");

    // 1. Reset Palang (Realtime)
    String msgGate = "{\"status\": \"Terbuka\"}";
    client.publish(topic_gate, msgGate.c_str());
    isPalangTertutup = false;

    // 2. Hitung Segmen Terakhir & Simpan
    float v4 = jarak_4_ke_5 / ((float)(t5 - t4) / 1000000.0);
    if(sample_index < BATCH_SIZE) {
        myPacket.data_kecepatan[sample_index] = v4;
        sample_index++;
        Serial.printf("   [Buffer] Data 4 disimpan: %.2f cm/s\n", v4);
    }

    // 3. KIRIM PAKET BINER SEKALIGUS (Sesuai PDF)
    // Mengirim seluruh struct myPacket (Start Time + 4 Data Kecepatan)
    Serial.println(">> MENGIRIM DATA BATCH KE MQTT...");
    
    client.publish(topic_batch, (uint8_t*)&myPacket, sizeof(myPacket));
    
    Serial.print("   [Sent] Ukuran Paket: ");
    Serial.print(sizeof(myPacket));
    Serial.println(" bytes");

    Serial.println("------------------------------\n");
    delay(2000);
  }
}