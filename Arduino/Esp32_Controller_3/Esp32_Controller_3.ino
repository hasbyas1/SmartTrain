#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "ICT-LAB WORKSPACE";
const char* password = "ICTLAB2024";

// HiveMQ Cloud credentials
const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Device02";
const char* mqtt_pass = "Device02";
const char* mqtt_topic_barrier = "smarttrain/palang";
const char* mqtt_topic_camera = "smarttrain/camera";

// Pin configuration for ESP32
#define SERVO_PIN_1       33  // Servo 1
#define SERVO_PIN_2       25  // Servo 2
#define BUZZER_PIN        32  // Buzzer
#define CAM_WAKEUP_PIN    26  // Pin untuk BANGUNKAN ESP32-CAM (ke Pin 14)
#define CAM_SLEEP_PIN     27  // Pin untuk TIDURKAN ESP32-CAM (ke Pin 15)

// Servo positions
const int BARRIER_UP = 90;
const int BARRIER_DOWN = 0;

// Objects
Servo barrierServo1;
Servo barrierServo2;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// State variables
bool barrierDown = false;
bool servosAttached = false;

// Anti-loop protection
unsigned long lastBarrierTime = 0;
unsigned long lastCameraTime = 0;
String lastBarrierMsg = "";
String lastCameraMsg = "";

// HiveMQ Cloud root certificate
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

// Function to sound buzzer before barrier movement
void soundBuzzer() {
    // Beep 3 kali untuk peringatan
    for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
    }
}

// Function to attach servos (hemat daya)
void attachServos() {
    if (!servosAttached) {
        Serial.println("Attaching servos...");
        barrierServo1.attach(SERVO_PIN_1);
        barrierServo2.attach(SERVO_PIN_2);
        servosAttached = true;
        delay(50);
    }
}

// Function to detach servos (hemat daya)
void detachServos() {
    if (servosAttached) {
        Serial.println("Detaching servos (power save)...");
        barrierServo1.detach();
        barrierServo2.detach();
        servosAttached = false;
    }
}

// Function to wake up ESP32-CAM via GPIO (Pin 14)
void wakeupCamera() {
    Serial.println("Waking up ESP32-CAM via GPIO...");
    
    // Kirim HIGH pulse ke Pin 14 ESP32-CAM
    digitalWrite(CAM_WAKEUP_PIN, HIGH);
    delay(500);  // Pulse 500ms
    digitalWrite(CAM_WAKEUP_PIN, LOW);
    
    Serial.println("Camera wakeup signal sent (GPIO pulse to Pin 14)");
    Serial.println("Waiting for camera to boot (5 seconds)...");
    delay(5000);  // Tunggu ESP32-CAM boot & connect WiFi
    Serial.println("Camera should be ready now!");
}

// Function to put ESP32-CAM to sleep via GPIO (Pin 15)
void sleepCamera() {
    Serial.println("Sending sleep signal to ESP32-CAM via GPIO...");
    
    // Kirim HIGH pulse ke Pin 15 ESP32-CAM
    digitalWrite(CAM_SLEEP_PIN, HIGH);
    delay(500);  // Pulse 500ms
    digitalWrite(CAM_SLEEP_PIN, LOW);
    
    Serial.println("Camera sleep signal sent (GPIO pulse to Pin 15)");
    Serial.println("Camera will enter deep sleep in 2 seconds...");
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("MQTT received: ");
    Serial.println(message);
    
    // ============================================================
    // Topic: smarttrain/palang (Kontrol Palang)
    // ============================================================
    if (strcmp(topic, mqtt_topic_barrier) == 0) {
        // Anti-loop: cek debounce dan duplicate
        if ((millis() - lastBarrierTime < 2000) || (message == lastBarrierMsg && millis() - lastBarrierTime < 5000)) {
            Serial.println("Barrier command ignored (anti-loop)");
            return;
        }
        
        // Parse JSON sederhana
        if (message.indexOf("Tertutup") >= 0) {
            soundBuzzer();  // Buzzer berbunyi dulu
            
            // Attach servos
            attachServos();
            
            barrierServo1.write(BARRIER_DOWN);
            barrierServo2.write(BARRIER_DOWN);
            barrierDown = true;
            Serial.println("Barrier DOWN");
            
            delay(600); // Tunggu gerakan selesai
            
            // Detach servos untuk hemat daya
            detachServos();
            
            // Update tracking
            lastBarrierTime = millis();
            lastBarrierMsg = message;
            
        } else if (message.indexOf("Terbuka") >= 0) {
            soundBuzzer();  // Buzzer berbunyi dulu
            
            // Attach servos
            attachServos();
            
            barrierServo1.write(BARRIER_UP);
            barrierServo2.write(BARRIER_UP);
            barrierDown = false;
            Serial.println("Barrier UP");
            
            delay(600); // Tunggu gerakan selesai
            
            // Detach servos untuk hemat daya
            detachServos();
            
            // Update tracking
            lastBarrierTime = millis();
            lastBarrierMsg = message;
        }
    }
    
    // ============================================================
    // Topic: smarttrain/camera (Kontrol Kamera)
    // ============================================================
    else if (strcmp(topic, mqtt_topic_camera) == 0) {
        // Anti-loop: cek debounce dan duplicate
        if ((millis() - lastCameraTime < 2000) || (message == lastCameraMsg && millis() - lastCameraTime < 5000)) {
            Serial.println("Camera command ignored (anti-loop)");
            return;
        }
        
        // Parse command
        if (message.indexOf("Aktif") >= 0) {
            wakeupCamera();  // GPIO wake
            
            // Update tracking
            lastCameraTime = millis();
            lastCameraMsg = message;
            
        } else if (message.indexOf("Nonaktif") >= 0) {
            sleepCamera();  // GPIO sleep
            
            // Update tracking
            lastCameraTime = millis();
            lastCameraMsg = message;
        }
    }
}

// Connect to MQTT
void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        String clientId = "ESP32-" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("connected");
            mqttClient.subscribe(mqtt_topic_barrier);
            mqttClient.subscribe(mqtt_topic_camera);
            Serial.print("Subscribed to: ");
            Serial.print(mqtt_topic_barrier);
            Serial.print(" & ");
            Serial.println(mqtt_topic_camera);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP32 Barrier Controller (Full GPIO Control)...");

    // Initialize pins
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CAM_WAKEUP_PIN, OUTPUT);
    pinMode(CAM_SLEEP_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(CAM_WAKEUP_PIN, LOW);
    digitalWrite(CAM_SLEEP_PIN, LOW);
    
    // Servos will be attached on-demand (power save)
    Serial.println("Servos in detached mode (power save)");

    // WiFi connection
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Setup MQTT
    espClient.setCACert(root_ca);
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    
    connectMQTT();

    Serial.print("Ready! IP: ");
    Serial.println(WiFi.localIP());
    
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    Serial.println("========================================");
    Serial.println("FULL GPIO CONTROL - No HTTP needed!");
    Serial.println("========================================");
    Serial.println("MQTT Topics:");
    Serial.println("  smarttrain/palang:");
    Serial.println("    {\"status\":\"Tertutup\"}  -> Lower barrier");
    Serial.println("    {\"status\":\"Terbuka\"}   -> Raise barrier");
    Serial.println("  smarttrain/camera:");
    Serial.println("    {\"status\":\"Aktif\"}     -> Wake camera (GPIO)");
    Serial.println("    {\"status\":\"Nonaktif\"} -> Sleep camera (GPIO)");
    Serial.println("========================================");
    Serial.println("Camera Control Pins:");
    Serial.println("  GPIO 26 -> Pin 14 (Wake)");
    Serial.println("  GPIO 27 -> Pin 15 (Sleep)");
    Serial.println("========================================");
    
    // AUTO-WAKE CAMERA saat ESP32 startup
    Serial.println("Auto-waking camera on startup...");
    delay(2000); // Tunggu MQTT stabil
    wakeupCamera();
    mqttClient.publish(mqtt_topic_camera, "{\"status\":\"Aktif\"}");
    Serial.println("Camera auto-wake complete!");
}

void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
    
    delay(10);
}

/*
 * ============================================================================
 * WIRING ESP32 38-PIN - FULL GPIO CONTROL:
 * ============================================================================
 * GPIO 33 → Servo 1 Signal
 * GPIO 25 → Servo 2 Signal
 * GPIO 32 → Buzzer +
 * GPIO 26 → ESP32-CAM Pin 14 (Wake trigger)
 * GPIO 27 → ESP32-CAM Pin 15 (Sleep trigger)
 * 5V      → Servo Power
 * GND     → Common Ground (ESP32 + ESP32-CAM)
 * 
 * ============================================================================
 * ADVANTAGES - FULL GPIO:
 * ============================================================================
 * ✅ Tidak perlu tahu IP address ESP32-CAM
 * ✅ Tidak perlu HTTP/WiFi untuk sleep
 * ✅ Lebih cepat (hardware trigger)
 * ✅ Lebih reliable (tidak depend on network)
 * ✅ Konsisten (wake & sleep pakai metode sama)
 * 
 * ============================================================================
 * HOW IT WORKS:
 * ============================================================================
 * 
 * WAKE (GPIO 26 -> Pin 14):
 *   1. ESP32 kirim HIGH pulse (500ms) ke GPIO 26
 *   2. ESP32-CAM Pin 14 detect HIGH
 *   3. ESP32-CAM bangun dari deep sleep
 *   4. Boot + WiFi connect (~5 detik)
 *   5. HTTP server ready
 * 
 * SLEEP (GPIO 27 -> Pin 15):
 *   1. ESP32 kirim HIGH pulse (500ms) ke GPIO 27
 *   2. ESP32-CAM Pin 15 detect HIGH (via interrupt)
 *   3. ESP32-CAM trigger sleep routine
 *   4. Stop HTTP server, disconnect WiFi
 *   5. Enter deep sleep (~10μA)
 * 
 * ============================================================================
 */
