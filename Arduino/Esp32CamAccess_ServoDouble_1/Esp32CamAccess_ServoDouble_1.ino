#include <WiFi.h>
#include "esp_camera.h"
#include <ESP32Servo.h>
#include <WebServer.h>

// ================= KONFIGURASI WIFI =================
const char* ssid = "EsDehidrasiHouse";     // GANTI INI
const char* password = "11011011"; // GANTI INI

// ================= KONFIGURASI 2 SERVO =================
Servo servo1;
Servo servo2;

const int pinServo1 = 14;  // GPIO 14
const int pinServo2 = 15;  // GPIO 15 (Aman jika tanpa SD Card)

// ================= KONFIGURASI KAMERA =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Kamera Siap!");
}

// Tampilan Web Browser
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif; text-align:center; margin-top:50px;} button{padding:10px 20px; margin:5px; font-size:16px;}</style></head><body>";
  
  html += "<h1>KONTROL 2 SERVO</h1>";
  
  // Bagian Servo 1
  html += "<h3>Servo 1 (Pin 14)</h3>";
  html += "<a href=\"/servo?id=1&angle=0\"><button>0&deg;</button></a>";
  html += "<a href=\"/servo?id=1&angle=90\"><button>90&deg;</button></a>";
  html += "<a href=\"/servo?id=1&angle=180\"><button>180&deg;</button></a>";

  html += "<hr>";

  // Bagian Servo 2
  html += "<h3>Servo 2 (Pin 15)</h3>";
  html += "<a href=\"/servo?id=2&angle=0\"><button>0&deg;</button></a>";
  html += "<a href=\"/servo?id=2&angle=90\"><button>90&deg;</button></a>";
  html += "<a href=\"/servo?id=2&angle=180\"><button>180&deg;</button></a>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Logic Penggerak Servo
void handleServo() {
  if (server.hasArg("id") && server.hasArg("angle")) {
    int id = server.arg("id").toInt();
    int angle = server.arg("angle").toInt();

    // Batasi sudut
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    if (id == 1) {
      servo1.write(angle);
      Serial.print("Servo 1 gerak ke: ");
    } else if (id == 2) {
      servo2.write(angle);
      Serial.print("Servo 2 gerak ke: ");
    }
    Serial.println(angle);
    
    // Redirect kembali ke halaman utama agar tombol bisa ditekan lagi
    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    server.send(400, "text/plain", "Error: Parameter missing");
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Setup PWM untuk 2 Servo
  // Alokasi timer agar tidak bentrok
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servo1.setPeriodHertz(50);
  servo1.attach(pinServo1, 500, 2400); // Servo 1 di Pin 14
  servo1.write(90); // Posisi Awal Tengah

  servo2.setPeriodHertz(50);
  servo2.attach(pinServo2, 500, 2400); // Servo 2 di Pin 15
  servo2.write(90); // Posisi Awal Tengah

  // 2. Init Kamera
  initCamera();

  // 3. Init WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  Serial.print("Buka browser di: http://");
  Serial.println(WiFi.localIP());

  // 4. Server Route
  server.on("/", handleRoot);
  server.on("/servo", handleServo);
  server.begin();
}

void loop() {
  server.handleClient();
}