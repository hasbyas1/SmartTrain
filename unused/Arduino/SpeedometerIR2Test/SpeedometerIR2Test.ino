// ==========================================
//  SPEEDOMETER KERETA (2 SENSOR IR) - ESP32
// ==========================================

// --- KONFIGURASI PIN ---
const int pinSensor1 = 13; // Sensor Pertama (Start)
const int pinSensor2 = 12; // Sensor Kedua (Finish)

// --- KONSTANTA JARAK ---
// Jarak antar sensor dalam METER (12.5 cm = 0.125 m)
const float JARAK_ANTAR_SENSOR = 0.125; 

// Variabel untuk menyimpan waktu
unsigned long waktuMulai = 0;
unsigned long waktuSelesai = 0;
bool sedangMengukur = false; // Status apakah pengukuran sedang berjalan

void setup() {
  // Mulai komunikasi Serial untuk melihat hasil di layar komputer
  Serial.begin(115200);
  
  // Atur pin sensor sebagai input
  pinMode(pinSensor1, INPUT);
  pinMode(pinSensor2, INPUT);

  Serial.println("=== Sistem Siap ===");
  Serial.println("Menunggu kereta melewati Sensor 1...");
}

void loop() {
  // 1. Cek apakah Sensor 1 mendeteksi kereta (Logika LOW = Ada Benda)
  // Kita hanya mulai jika BELUM sedang mengukur
  if (digitalRead(pinSensor1) == LOW && sedangMengukur == false) {
    waktuMulai = micros(); // Catat waktu sekarang dalam mikrodetik
    sedangMengukur = true; // Ubah status menjadi sedang mengukur
    Serial.println("-> Timer Dimulai! Menunggu Sensor 2...");
    
    // Debounce sederhana: Tunggu sebentar agar tidak mendeteksi gerbong yang sama berulang kali
    delay(50); 
  }

  // 2. Cek apakah Sensor 2 mendeteksi kereta
  // Kita hanya cek jika SEDANG mengukur (artinya sensor 1 sudah dilewati)
  if (digitalRead(pinSensor2) == LOW && sedangMengukur == true) {
    waktuSelesai = micros(); // Catat waktu selesai
    
    hitungKecepatan(); // Panggil fungsi hitung
    
    sedangMengukur = false; // Reset status untuk kereta berikutnya
    Serial.println("---------------------------------");
    Serial.println("Siap untuk putaran berikutnya...");
    
    // Delay agak lama agar seluruh rangkaian kereta lewat dulu
    delay(2000); 
  }
}

void hitungKecepatan() {
  // Hitung selisih waktu (durasi)
  unsigned long durasiMicros = waktuSelesai - waktuMulai;
  
  // Ubah durasi ke detik (1 detik = 1.000.000 mikrodetik)
  float durasiDetik = durasiMicros / 1000000.0;

  // Rumus: Kecepatan = Jarak / Waktu
  float kecepatanMps = JARAK_ANTAR_SENSOR / durasiDetik; // Meter per detik
  float kecepatanKmh = kecepatanMps * 3.6;               // Konversi ke Km/jam

  // Tampilkan hasil
  Serial.print("Waktu Tempuh : "); 
  Serial.print(durasiDetik, 4); 
  Serial.println(" detik");

  Serial.print("Kecepatan    : "); 
  Serial.print(kecepatanMps); 
  Serial.print(" m/s  ( ");
  Serial.print(kecepatanKmh);
  Serial.println(" km/jam )");
}