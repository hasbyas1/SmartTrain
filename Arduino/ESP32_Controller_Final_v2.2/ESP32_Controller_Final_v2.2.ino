/*
 * ============================================================================
 * SMART TRAIN LEVEL CROSSING SYSTEM - ESP32 CONTROLLER
 * ============================================================================
 * Version: 2.0 Final
 * Device: ESP32 DevKit v1 (38-pin)
 * 
 * HARDWARE:
 * - 2x Servo motor (SG90 atau MG996R)
 * - 1x Active buzzer
 * - 1x Relay module 5V (1 channel)
 * - 1x OLED Display 128x64 I2C (SH1106 atau SSD1306)
 * 
 * PIN CONFIGURATION:
 * ────────────────────────────────────────────────────────────────────────────
 * GPIO 25  →  Servo 1 Signal (Palang kiri)
 * GPIO 26  →  Servo 2 Signal (Palang kanan)
 * GPIO 14  →  Buzzer + (Active buzzer)
 * GPIO 27  →  Relay IN (Power control ESP32-CAM)
 * GPIO 33  →  OLED SDA (I2C Data)
 * GPIO 32  →  OLED SCL (I2C Clock)
 * 5V       →  Servo VCC, Relay VCC, OLED VCC
 * GND      →  Common Ground
 * ────────────────────────────────────────────────────────────────────────────
 * 
 * RELAY WIRING (PENTING!):
 * ────────────────────────────────────────────────────────────────────────────
 * Relay VCC  →  5V (ESP32)
 * Relay GND  →  GND (ESP32)
 * Relay IN   →  GPIO 27 (ESP32)
 * Relay COM  →  5V Power Supply (+)
 * Relay NO   →  ESP32-CAM 5V Pin
 * ────────────────────────────────────────────────────────────────────────────
 * 
 * MQTT TOPICS:
 * - smartTrain/barrier      (SUB & PUB) - Kontrol palang
 * - smartTrain/camera       (SUB & PUB) - Kontrol relay kamera
 * - smartTrain/camera/ip    (SUB)       - Terima IP dari ESP32-CAM
 * - smartTrain/engine       (FUTURE)    - Kontrol kecepatan kereta
 * 
 * ============================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
// #include <Adafruit_SH110X.h>  // Untuk OLED SH1106
#include <Adafruit_SSD1306.h> // Jika pakai SSD1306

// ============================================================================
// WIFI CONFIGURATION - MULTI CREDENTIALS
// ============================================================================

// WiFi credentials - Primary & Backup
const char* ssid_primary = "EsDehidrasiHouse";
const char* password_primary = "11011011";

const char* ssid_backup1 = "EsDehidrasi";
const char* password_backup1 = "11011011";

const char* ssid_backup2 = "ICT-LAB WORKSPACE";
const char* password_backup2 = "ICTLAB2024";

// WiFi retry configuration
const int WIFI_RETRY_DELAY = 500;   // 500ms per retry
const int WIFI_MAX_RETRY = 10;      // Max 10 retry per WiFi (5 detik)

// ============================================================================
// CONFIGURATION
// ============================================================================

// HiveMQ Cloud credentials
const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Device02";
const char* mqtt_pass = "Device02";

// MQTT Topics
const char* mqtt_topic_barrier = "smartTrain/barrier";
const char* mqtt_topic_camera = "smartTrain/camera";
const char* mqtt_topic_camera_ip = "smartTrain/camera/ip";
// const char* mqtt_topic_engine = "smartTrain/engine";  // FUTURE: Speed control

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define SERVO_PIN_1       25  // Servo 1 (Palang kiri)
#define SERVO_PIN_2       26  // Servo 2 (Palang kanan)
#define BUZZER_PIN        14  // Buzzer
#define RELAY_CAM_PIN     27  // Relay untuk power ESP32-CAM (Low-Trigger, Relay Aktif ketika Pin LOW)
#define OLED_SDA          33  // OLED I2C SDA
#define OLED_SCL          32  // OLED I2C SCL

// Servo positions
const int BARRIER_UP = 90;    // Palang naik (0-180)
const int BARRIER_DOWN = 0;   // Palang turun

// ============================================================================
// OBJECTS & VARIABLES
// ============================================================================

// Hardware objects
Servo barrierServo1;
Servo barrierServo2;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// OLED Display (128x64, I2C address 0x3C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
// Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Untuk SSD1306, ganti dengan:
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State variables
bool barrierDown = false;      // Status palang (true=turun, false=naik)
bool servosAttached = false;   // Status servo (attached/detached)
bool cameraActive = false;     // Status relay kamera
String esp32camIP = "Waiting...";  // IP ESP32-CAM
String controllerIP = "";      // IP Controller

// Anti-loop protection
unsigned long lastBarrierTime = 0;
unsigned long lastCameraTime = 0;
String lastBarrierMsg = "";
String lastCameraMsg = "";

// OLED update interval
unsigned long lastOLEDUpdate = 0;
const unsigned long OLED_UPDATE_INTERVAL = 500; // 500ms

// ============================================================================
// SSL CERTIFICATE
// ============================================================================
static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// ============================================================================
// OLED DISPLAY FUNCTIONS
// ============================================================================

void initOLED() {
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // Untuk SH1106
    // if (!display.begin(0x3C, true)) {
    //     Serial.println("❌ OLED initialization failed!");
    //     return;
    // }
    
    // Untuk SSD1306, gunakan:
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("❌ OLED initialization failed!");
        return;
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    // dt isplay.setTextColor(SH110X_WHITE);
    // Untuk SSD1306:
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.println("Smart Train System");
    display.println("Initializing...");
    display.display();
    
    Serial.println("✅ OLED initialized");
}

void updateOLED() {
    display.clearDisplay();
    display.setTextSize(1);
    // display.setTextColor(SH110X_WHITE);
    // Untuk SSD1306:
    display.setTextColor(SSD1306_WHITE);
    
    // Line 0: Title
    display.setCursor(0, 0);
    display.println("Smart Train Control");
    
    // Line 1: Controller IP
    display.setCursor(0, 10);
    display.print("Ctrl: ");
    display.println(controllerIP);
    
    // Line 2: Camera IP
    display.setCursor(0, 20);
    display.print("CAM:  ");
    display.println(esp32camIP);
    
    // Line 3: Barrier Status
    display.setCursor(0, 30);
    display.print("Palang: ");
    display.println(barrierDown ? "TERTUTUP" : "TERBUKA");
    
    // Line 4: Camera Status
    display.setCursor(0, 40);
    display.print("Kamera: ");
    display.println(cameraActive ? "AKTIF" : "NONAKTIF");
    
    // Line 5: MQTT Status
    display.setCursor(0, 50);
    display.print("MQTT: ");
    display.println(mqttClient.connected() ? "Connected" : "Disconnected");
    
    display.display();
}

// ============================================================================
// HARDWARE CONTROL FUNCTIONS
// ============================================================================

void soundBuzzer() {
    // Beep 3 kali sebelum gerakan palang
    for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
    }
}

void attachServos() {
    if (!servosAttached) {
        Serial.println("⚙️  Attaching servos...");
        barrierServo1.attach(SERVO_PIN_1);
        barrierServo2.attach(SERVO_PIN_2);
        servosAttached = true;
        delay(50);
    }
}

void detachServos() {
    if (servosAttached) {
        Serial.println("💤 Detaching servos (power save)...");
        barrierServo1.detach();
        barrierServo2.detach();
        servosAttached = false;
    }
}

void setCameraRelay(bool on) {
    digitalWrite(RELAY_CAM_PIN, on ? LOW : HIGH);
    cameraActive = on;
    
    Serial.print("📷 Camera relay: ");
    Serial.println(on ? "ON (powered)" : "OFF (powered down)");
    
    if (!on) {
        esp32camIP = "Offline";  // Reset IP saat kamera mati
    }
}

// ============================================================================
// MQTT CALLBACK FUNCTION
// ============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("📨 MQTT [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    
    // ========================================================================
    // Topic: smartTrain/barrier (Kontrol Palang)
    // ========================================================================
    if (strcmp(topic, mqtt_topic_barrier) == 0) {
        // Anti-loop: debounce & duplicate check
        if ((millis() - lastBarrierTime < 2000) || 
            (message == lastBarrierMsg && millis() - lastBarrierTime < 5000)) {
            Serial.println("⚠️  Barrier command ignored (anti-loop protection)");
            return;
        }
        
        // Command: Tertutup (Close barrier)
        if (message.indexOf("Tertutup") >= 0) {
            Serial.println("🚧 Closing barrier...");
            soundBuzzer();
            attachServos();
            
            barrierServo1.write(BARRIER_DOWN);
            barrierServo2.write(BARRIER_DOWN);
            barrierDown = true;
            
            delay(600);  // Tunggu gerakan selesai
            detachServos();
            
            // Publish status
            // mqttClient.publish(mqtt_topic_barrier, "{\"status\":\"Tertutup\"}");
            Serial.println("✅ Barrier DOWN");
            
            lastBarrierTime = millis();
            lastBarrierMsg = message;
        }
        // Command: Terbuka (Open barrier)
        else if (message.indexOf("Terbuka") >= 0) {
            Serial.println("🚧 Opening barrier...");
            soundBuzzer();
            attachServos();
            
            barrierServo1.write(BARRIER_UP);
            barrierServo2.write(BARRIER_UP);
            barrierDown = false;
            
            delay(600);  // Tunggu gerakan selesai
            detachServos();
            
            // Publish status
            // mqttClient.publish(mqtt_topic_barrier, "{\"status\":\"Terbuka\"}");
            Serial.println("✅ Barrier UP");
            
            lastBarrierTime = millis();
            lastBarrierMsg = message;
        }
    }
    
    // ========================================================================
    // Topic: smartTrain/camera (Kontrol Relay Kamera)
    // ========================================================================
    else if (strcmp(topic, mqtt_topic_camera) == 0) {
        // Anti-loop: debounce & duplicate check
        if ((millis() - lastCameraTime < 2000) || 
            (message == lastCameraMsg && millis() - lastCameraTime < 5000)) {
            Serial.println("⚠️  Camera command ignored (anti-loop protection)");
            return;
        }
        
        // Command: Aktif (Turn ON camera)
        if (message.indexOf("Aktif") >= 0) {
            Serial.println("📷 Turning ON camera...");
            setCameraRelay(true);
            
            // Publish status
            // mqttClient.publish(mqtt_topic_camera, "{\"status\":\"Aktif\"}");
            Serial.println("✅ Camera ON (waiting for boot ~5s)");
            
            lastCameraTime = millis();
            lastCameraMsg = message;
        }
        // Command: Nonaktif (Turn OFF camera)
        else if (message.indexOf("Nonaktif") >= 0) {
            Serial.println("📷 Turning OFF camera...");
            setCameraRelay(false);
            
            // Publish status
            // mqttClient.publish(mqtt_topic_camera, "{\"status\":\"Nonaktif\"}");
            Serial.println("✅ Camera OFF (powered down)");
            
            lastCameraTime = millis();
            lastCameraMsg = message;
        }
    }
    
    // ========================================================================
    // Topic: smartTrain/camera/ip (Terima IP dari ESP32-CAM)
    // ========================================================================
    else if (strcmp(topic, mqtt_topic_camera_ip) == 0) {
        // Parse JSON: {"ip":"192.168.1.100"}
        int startIdx = message.indexOf("\"ip\":\"") + 6;
        int endIdx = message.indexOf("\"", startIdx);
        
        if (startIdx > 5 && endIdx > startIdx) {
            esp32camIP = message.substring(startIdx, endIdx);
            Serial.print("📷 ESP32-CAM IP updated: ");
            Serial.println(esp32camIP);
        }
    }
    
    // ========================================================================
    // Topic: smartTrain/engine (FUTURE - Kontrol Kecepatan Kereta)
    // ========================================================================
    // COMMENT DULU - Untuk future implementation
    // Logic: Python detect object + palang tertutup → publish speed command
    // ESP32 Controller hanya subscribe untuk display di OLED (optional)
    
    // else if (strcmp(topic, mqtt_topic_engine) == 0) {
    //     // Parse speed command dari Python
    //     // Format: {"speed":"lambat"} atau {"speed":"berhenti"} atau {"speed":"normal"}
    //     
    //     // Optional: Display speed di OLED
    //     Serial.print("🚂 Train speed command: ");
    //     Serial.println(message);
    //     
    //     // Atau forward ke ESP32 Kereta via MQTT (jika ada)
    // }
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("🔌 Connecting to MQTT...");
        String clientId = "ESP32Controller-" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println(" ✅ Connected!");
            
            // Subscribe to topics
            mqttClient.subscribe(mqtt_topic_barrier);
            mqttClient.subscribe(mqtt_topic_camera);
            mqttClient.subscribe(mqtt_topic_camera_ip);
            // mqttClient.subscribe(mqtt_topic_engine);  // FUTURE
            
            Serial.println("📡 Subscribed to:");
            Serial.print("   - ");
            Serial.println(mqtt_topic_barrier);
            Serial.print("   - ");
            Serial.println(mqtt_topic_camera);
            Serial.print("   - ");
            Serial.println(mqtt_topic_camera_ip);
            // Serial.print("   - ");
            // Serial.println(mqtt_topic_engine);  // FUTURE
            
        } else {
            Serial.print(" ❌ Failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" (retrying in 5 seconds...)");
            delay(5000);
        }
    }
}

// ============================================================================
// WIFI MULTI-CREDENTIALS CONNECTION
// ============================================================================

void connectWiFiMulti() {
    Serial.println("📶 Connecting to WiFi (Multi-Credentials)...");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    
    // ========================================================================
    // Try WiFi 1 (Primary)
    // ========================================================================
    Serial.print("Trying WiFi 1: ");
    Serial.println(ssid_primary);
    WiFi.begin(ssid_primary, password_primary);
    
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < WIFI_MAX_RETRY) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retry++;
    }
    
    // ========================================================================
    // If failed, try WiFi 2 (Backup 1)
    // ========================================================================
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n⚠️  WiFi 1 failed! Trying WiFi 2...");
        WiFi.disconnect();
        delay(1000);
        
        Serial.print("Trying WiFi 2: ");
        Serial.println(ssid_backup1);
        WiFi.begin(ssid_backup1, password_backup1);
        
        retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < WIFI_MAX_RETRY) {
            delay(WIFI_RETRY_DELAY);
            Serial.print(".");
            retry++;
        }
    }
    
    // ========================================================================
    // If failed, try WiFi 3 (Backup 2)
    // ========================================================================
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n⚠️  WiFi 2 failed! Trying WiFi 3...");
        WiFi.disconnect();
        delay(1000);
        
        Serial.print("Trying WiFi 3: ");
        Serial.println(ssid_backup2);
        WiFi.begin(ssid_backup2, password_backup2);
        
        retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < WIFI_MAX_RETRY) {
            delay(WIFI_RETRY_DELAY);
            Serial.print(".");
            retry++;
        }
    }
    
    // ========================================================================
    // Check final connection status
    // ========================================================================
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("📶 SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("📍 IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ All WiFi failed!");
        Serial.println("   Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("🚂 SMART TRAIN LEVEL CROSSING SYSTEM");
    Serial.println("    ESP32 Controller (38-pin)");
    Serial.println("========================================");

    // Initialize OLED
    initOLED();

    // Initialize GPIO pins
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RELAY_CAM_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_CAM_PIN, HIGH);  // Camera OFF saat startup (LOW TRIGGER)
    
    Serial.println("✅ Pins initialized");
    Serial.println("✅ Servos in detached mode (power save)");
    Serial.println("✅ Camera relay OFF (powered down)");

    // ========================================================================
    // WiFi connection - UPDATED: Multi-Credentials
    // ========================================================================
    connectWiFiMulti();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi connection failed!");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("WiFi FAILED!");
        display.display();
        while(1);  // Halt
    }
    
    controllerIP = WiFi.localIP().toString();
    // ========================================================================

    // Setup MQTT
    espClient.setCACert(root_ca);
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    
    connectMQTT();
    
    Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.println("📋 MQTT Topics:");
    Serial.println("   smartTrain/barrier   - Barrier control");
    Serial.println("   smartTrain/camera    - Camera relay");
    Serial.println("   smartTrain/camera/ip - Camera IP");
    // Serial.println("   smartTrain/engine    - Train speed (FUTURE)");
    Serial.println("========================================");
    Serial.println("✅ SYSTEM READY!");
    Serial.println("========================================\n");
    
    // Initial OLED update
    updateOLED();
    
    // Test buzzer
    Serial.println("🔔 Testing buzzer...");
    soundBuzzer();
    Serial.println("✅ Buzzer test complete!\n");

    // Test Servo
    Serial.println("🔧 Testing servo...");
    attachServos();        
    barrierServo1.write(BARRIER_DOWN);
    barrierServo2.write(BARRIER_DOWN);
    barrierDown = true;
    delay(600);  // Tunggu gerakan selesai

    barrierServo1.write(BARRIER_UP);
    barrierServo2.write(BARRIER_UP);
    barrierDown = false;
    delay(600);  // Tunggu gerakan selesai

    detachServos();
    Serial.println("✅ Servo test complete!\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        Serial.println("⚠️  MQTT disconnected! Reconnecting...");
        connectMQTT();
    }
    mqttClient.loop();
    
    // Update OLED every 500ms
    if (millis() - lastOLEDUpdate > OLED_UPDATE_INTERVAL) {
        updateOLED();
        lastOLEDUpdate = millis();
    }
    
    delay(10);
}

/*
 * ============================================================================
 * TESTING COMMANDS (via MQTT)
 * ============================================================================
 * 
 * 1. Close Barrier:
 *    Topic: smartTrain/barrier
 *    Message: {"status":"Tertutup"}
 *    Result: Buzzer beep 3x, servo turun
 * 
 * 2. Open Barrier:
 *    Topic: smartTrain/barrier
 *    Message: {"status":"Terbuka"}
 *    Result: Buzzer beep 3x, servo naik
 * 
 * 3. Turn ON Camera:
 *    Topic: smartTrain/camera
 *    Message: {"status":"Aktif"}
 *    Result: Relay ON, ESP32-CAM powered
 * 
 * 4. Turn OFF Camera:
 *    Topic: smartTrain/camera
 *    Message: {"status":"Nonaktif"}
 *    Result: Relay OFF, ESP32-CAM powered down
 * 
 * ============================================================================
 * LIBRARIES REQUIRED:
 * ============================================================================
 * - ESP32Servo (by Kevin Harrington)
 * - PubSubClient (by Nick O'Leary)
 * - Adafruit GFX Library
 * - Adafruit SH110X (untuk OLED SH1106)
 *   ATAU
 *   Adafruit SSD1306 (untuk OLED SSD1306)
 * 
 * Install via: Tools → Manage Libraries
 * 
 * ============================================================================
 */
