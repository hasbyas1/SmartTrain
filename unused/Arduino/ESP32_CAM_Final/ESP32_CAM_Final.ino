/*
 * ============================================================================
 * SMART TRAIN LEVEL CROSSING SYSTEM - ESP32-CAM MODULE
 * ============================================================================
 * Version: 2.0 Final
 * Device: ESP32-CAM AI-Thinker
 * 
 * FUNGSI:
 * - HTTP Server untuk video streaming & snapshot
 * - Publish IP address via MQTT saat boot
 * - Untuk object detection via Python (YOLO)
 * 
 * POWER CONTROL:
 * ────────────────────────────────────────────────────────────────────────────
 * ESP32-CAM powered by Relay (controlled by ESP32 Controller GPIO 27)
 * - Relay ON  → ESP32-CAM gets 5V power → Boot → WiFi → HTTP Server
 * - Relay OFF → ESP32-CAM powered down
 * ────────────────────────────────────────────────────────────────────────────
 * 
 * PIN CONFIGURATION (Fixed AI-Thinker):
 * ────────────────────────────────────────────────────────────────────────────
 * POWER PINS:
 * 5V   →  Dari Relay NO (Normally Open)
 * GND  →  Common Ground dengan ESP32 Controller
 * 
 * CAMERA PINS (Internal, tidak perlu wiring):
 * GPIO 0   →  XCLK
 * GPIO 26  →  SIOD (I2C Data)
 * GPIO 27  →  SIOC (I2C Clock)
 * GPIO 35  →  Y9 (MSB)
 * GPIO 34  →  Y8
 * GPIO 39  →  Y7
 * GPIO 36  →  Y6
 * GPIO 21  →  Y5
 * GPIO 19  →  Y4
 * GPIO 18  →  Y3
 * GPIO 5   →  Y2 (LSB)
 * GPIO 25  →  VSYNC
 * GPIO 23  →  HREF
 * GPIO 22  →  PCLK
 * GPIO 32  →  PWDN (Power Down)
 * GPIO -1  →  RESET (not used)
 * ────────────────────────────────────────────────────────────────────────────
 * 
 * MQTT TOPIC:
 * - smartTrain/camera/ip (PUBLISH only)
 *   Format: {"ip":"192.168.1.100"}
 *   Publish saat boot untuk update OLED di ESP32 Controller
 * 
 * HTTP ENDPOINTS:
 * - GET /         → Homepage
 * - GET /stream   → MJPEG video stream (untuk Python YOLO)
 * - GET /capture  → Single JPEG snapshot
 * - GET /status   → JSON status
 * 
 * CAMERA CONFIG:
 * - Frame size: QVGA (320x240) - optimal untuk YOLO
 * - JPEG quality: 12 (lower = better, range 10-63)
 * - Frame buffer: 1 (low memory footprint)
 * 
 * ============================================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi credentials
const char* ssid = "ICT-LAB WORKSPACE";
const char* password = "ICTLAB2024";

// HiveMQ Cloud credentials
const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Device02";
const char* mqtt_pass = "Device02";

// MQTT Topic - Publish IP address only
const char* mqtt_topic_camera_ip = "smartTrain/camera/ip";

// ============================================================================
// CAMERA PIN DEFINITIONS (AI-Thinker ESP32-CAM)
// ============================================================================
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

// ============================================================================
// OBJECTS & VARIABLES
// ============================================================================
httpd_handle_t stream_httpd = NULL;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

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
// HTTP HANDLERS
// ============================================================================

// Stream handler - MJPEG video stream untuk Python YOLO
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[128];

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=--123456789000000000000987654321");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send_chunk(req, "--123456789000000000000987654321\r\n", 37);

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("❌ Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!jpeg_converted) {
                Serial.println("❌ JPEG conversion failed");
                res = ESP_FAIL;
                break;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        size_t hlen = snprintf(part_buf, 128, 
            "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
        
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) break;

        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        if (res != ESP_OK) break;

        res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 39);
        if (res != ESP_OK) break;

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // ~10 FPS
    }

    if (fb) esp_camera_fb_return(fb);
    else if (_jpg_buf) free(_jpg_buf);
    
    return res;
}

// Capture handler - Single JPEG snapshot
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("❌ Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    
    return res;
}

// Status handler - JSON status
static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    char response[256];
    snprintf(response, sizeof(response), 
        "{\"status\":\"online\",\"ip\":\"%s\",\"heap\":%d,\"camera\":\"ESP32-CAM\"}",
        WiFi.localIP().toString().c_str(),
        ESP.getFreeHeap());
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Index handler - Homepage
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Smart Train ESP32-CAM</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 20px; }
        h1 { color: #333; }
        .button { 
            padding: 10px 20px; 
            margin: 10px; 
            font-size: 16px; 
            text-decoration: none; 
            color: white; 
            background-color: #4CAF50; 
            border-radius: 5px; 
            display: inline-block;
        }
        .info { 
            background-color: #f0f0f0; 
            padding: 15px; 
            margin: 20px auto; 
            max-width: 600px; 
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <h1>Smart Train ESP32-CAM</h1>
    <div class="info">
        <p><strong>Status:</strong> Online</p>
        <p><strong>Purpose:</strong> Object Detection Camera</p>
        <p><strong>Frame Size:</strong> QVGA (320x240)</p>
    </div>
    <a href="/stream" class="button">Video Stream</a>
    <a href="/capture" class="button">Capture</a>
    <a href="/status" class="button">Status</a>
    <div style="margin-top: 30px; color: #666;">
        <p>Endpoints:</p>
        <p>GET /stream - MJPEG stream</p>
        <p>GET /capture - Single snapshot</p>
        <p>GET /status - JSON status</p>
    </div>
</body>
</html>
)rawliteral";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// ============================================================================
// START HTTP SERVER
// ============================================================================

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.task_priority = 5;
    config.stack_size = 4096;
    config.max_uri_handlers = 6;
    config.max_open_sockets = 3;

    if (httpd_start(&stream_httpd, &config) != ESP_OK) {
        Serial.println("❌ Failed to start HTTP server");
        return;
    }

    httpd_uri_t uris[] = {
        {.uri = "/",        .method = HTTP_GET, .handler = index_handler},
        {.uri = "/stream",  .method = HTTP_GET, .handler = stream_handler},
        {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler},
        {.uri = "/status",  .method = HTTP_GET, .handler = status_handler}
    };

    for (int i = 0; i < 4; i++) {
        httpd_register_uri_handler(stream_httpd, &uris[i]);
    }
    
    Serial.println("✅ HTTP server started");
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

void connectMQTT() {
    int retry = 0;
    while (!mqttClient.connected() && retry < 5) {
        Serial.print("🔌 Connecting to MQTT...");
        String clientId = "ESP32CAM-" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println(" ✅ Connected!");
            
            // Publish IP address
            String ipMessage = "{\"ip\":\"" + WiFi.localIP().toString() + "\"}";
            mqttClient.publish(mqtt_topic_camera_ip, ipMessage.c_str());
            
            Serial.println("📡 Published IP to smartTrain/camera/ip");
            Serial.print("   IP: ");
            Serial.println(WiFi.localIP());
            
            break;
            
        } else {
            Serial.print(" ❌ Failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" (retrying...)");
            delay(2000);
            retry++;
        }
    }
    
    if (!mqttClient.connected()) {
        Serial.println("⚠️  MQTT connection failed after 5 retries");
        Serial.println("   Continuing with HTTP server only...");
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("📷 SMART TRAIN ESP32-CAM MODULE");
    Serial.println("    HTTP Server + MQTT IP Publisher");
    Serial.println("========================================");

    // Camera configuration
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
    
    // Frame size: QVGA (320x240) - optimal untuk YOLO object detection
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;  // 10-63, lower = better quality
    config.fb_count = 1;       // Single frame buffer (low memory)

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("❌ Camera init failed: 0x%x\n", err);
        Serial.println("   Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
        return;
    }
    Serial.println("✅ Camera initialized");
    Serial.println("   Frame size: QVGA (320x240)");
    Serial.println("   JPEG quality: 12");

    // WiFi connection
    Serial.print("📶 Connecting to WiFi");
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    int wifi_retry = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retry < 20) {
        delay(500);
        Serial.print(".");
        wifi_retry++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n❌ WiFi connection failed!");
        Serial.println("   Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
        return;
    }
    
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());

    // Start HTTP server
    startCameraServer();

    // Setup MQTT
    espClient.setCACert(root_ca);
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setBufferSize(512);
    
    // Connect to MQTT and publish IP
    connectMQTT();

    Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.println("📋 HTTP Endpoints:");
    Serial.print("   http://");
    Serial.print(WiFi.localIP());
    Serial.println("/         - Homepage");
    Serial.print("   http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream   - MJPEG stream");
    Serial.print("   http://");
    Serial.print(WiFi.localIP());
    Serial.println("/capture  - Single snapshot");
    Serial.print("   http://");
    Serial.print(WiFi.localIP());
    Serial.println("/status   - JSON status");
    Serial.println("========================================");
    Serial.println("✅ SYSTEM READY!");
    Serial.println("========================================\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Optional: Maintain MQTT connection untuk re-publish IP jika disconnect
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
    
    delay(100);
}

/*
 * ============================================================================
 * PYTHON INTEGRATION GUIDE
 * ============================================================================
 * 
 * Python program akan:
 * 1. Subscribe smartTrain/barrier (status palang: Terbuka/Tertutup)
 * 2. Subscribe smartTrain/camera/ip (IP ESP32-CAM: "192.168.1.100")
 * 3. HTTP GET http://<IP>/capture untuk ambil gambar
 * 4. YOLO object detection (detect bus, car, truck, dll)
 * 5. Decision logic:
 *    - IF palang tertutup AND vehicle detected:
 *        speed = "lambat" atau "berhenti"
 *    - ELSE IF palang terbuka:
 *        speed = "normal"
 * 6. Publish smartTrain/engine {"speed":"lambat"/"berhenti"/"normal"}
 * 
 * ============================================================================
 * EXAMPLE PYTHON CODE (YOLO + MQTT):
 * ============================================================================
 * 
 * import cv2
 * import requests
 * import paho.mqtt.client as mqtt
 * from ultralytics import YOLO
 * 
 * # Load YOLO model
 * model = YOLO('yolov8n.pt')
 * 
 * # MQTT setup
 * client = mqtt.Client()
 * client.username_pw_set("Device02", "Device02")
 * client.tls_set()
 * client.connect("9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud", 8883)
 * 
 * # Variables
 * camera_ip = ""
 * barrier_status = "Terbuka"
 * 
 * def on_message(client, userdata, msg):
 *     global camera_ip, barrier_status
 *     if msg.topic == "smartTrain/camera/ip":
 *         camera_ip = json.loads(msg.payload)["ip"]
 *     elif msg.topic == "smartTrain/barrier":
 *         barrier_status = json.loads(msg.payload)["status"]
 * 
 * client.on_message = on_message
 * client.subscribe("smartTrain/camera/ip")
 * client.subscribe("smartTrain/barrier")
 * client.loop_start()
 * 
 * while True:
 *     if camera_ip:
 *         # Get image from ESP32-CAM
 *         response = requests.get(f"http://{camera_ip}/capture")
 *         img_array = np.frombuffer(response.content, dtype=np.uint8)
 *         img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
 *         
 *         # YOLO detection
 *         results = model(img)
 *         vehicle_detected = any(cls in [2, 5, 7] for cls in results[0].boxes.cls)
 *         
 *         # Decision logic
 *         if barrier_status == "Tertutup" and vehicle_detected:
 *             speed = "lambat"
 *         elif barrier_status == "Tertutup":
 *             speed = "normal"
 *         else:
 *             speed = "normal"
 *         
 *         # Publish speed command
 *         client.publish("smartTrain/engine", f'{{"speed":"{speed}"}}')
 *     
 *     time.sleep(1)
 * 
 * ============================================================================
 * UPLOAD INSTRUCTIONS:
 * ============================================================================
 * 
 * 1. Wiring untuk Upload:
 *    ESP32-CAM  →  USB to TTL Programmer
 *    ─────────────────────────────────────
 *    5V         →  5V
 *    GND        →  GND
 *    U0R (RX)   →  TX
 *    U0T (TX)   →  RX
 *    IO0        →  GND (saat upload, lepas saat run!)
 * 
 * 2. Arduino IDE Settings:
 *    - Board: "AI Thinker ESP32-CAM"
 *    - Upload Speed: 115200
 *    - Flash Frequency: 80MHz
 *    - Partition Scheme: "Huge APP (3MB No OTA)"
 * 
 * 3. Upload Steps:
 *    a. Connect IO0 to GND
 *    b. Press RESET button on ESP32-CAM
 *    c. Click Upload in Arduino IDE
 *    d. Wait for upload to complete
 *    e. **LEPAS IO0 dari GND**
 *    f. Press RESET button again
 *    g. Open Serial Monitor (115200 baud)
 * 
 * ============================================================================
 * LIBRARIES REQUIRED:
 * ============================================================================
 * - PubSubClient (by Nick O'Leary)
 * 
 * Install via: Tools → Manage Libraries
 * 
 * ============================================================================
 * TROUBLESHOOTING:
 * ============================================================================
 * 
 * 1. "Brownout detector was triggered"
 *    → Power supply lemah, pakai 5V 2A external power
 *    → Jangan pakai power dari USB programmer
 * 
 * 2. Camera init failed
 *    → Check wiring kamera (internal)
 *    → Try different frame size (FRAMESIZE_SVGA)
 * 
 * 3. WiFi tidak connect
 *    → Check SSID dan password
 *    → Check jarak ke router
 * 
 * 4. MQTT tidak publish IP
 *    → Check HiveMQ credentials
 *    → Check internet connection
 *    → System tetap jalan dengan HTTP server
 * 
 * ============================================================================
 */
