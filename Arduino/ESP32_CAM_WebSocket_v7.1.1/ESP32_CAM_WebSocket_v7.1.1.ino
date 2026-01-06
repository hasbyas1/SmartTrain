/*
 * ============================================================================
 * SMART TRAIN LEVEL CROSSING SYSTEM - ESP32-CAM MODULE (WebSocket Version)
 * ============================================================================
 * Version: 3.0 WebSocket
 * Device: ESP32-CAM AI-Thinker
 * 
 * UPDATE v3.0:
 * - WebSocket server untuk real-time frame streaming
 * - Latency ultra-low (~50-100ms vs HTTP ~200-500ms)
 * - Two-way communication support
 * - Maintain MQTT IP publishing
 * - Backward compatible dengan HTTP endpoints
 * 
 * FUNGSI:
 * - WebSocket Server untuk real-time video streaming
 * - HTTP Server untuk backup & monitoring
 * - Publish IP address via MQTT (periodic)
 * - Untuk object detection via Python (YOLO)
 * 
 * WEBSOCKET PROTOCOL:
 * ────────────────────────────────────────────────────────────────────────────
 * Client → Server:
 * - "GET_FRAME"    → Request single frame
 * - "START_STREAM" → Start continuous streaming
 * - "STOP_STREAM"  → Stop streaming
 * - "GET_STATUS"   → Get camera status
 * 
 * Server → Client:
 * - Binary data    → JPEG frame
 * - JSON status    → Camera information
 * ────────────────────────────────────────────────────────────────────────────
 * 
 * LIBRARIES REQUIRED:
 * - PubSubClient (by Nick O'Leary)
 * - arduinoWebSockets (by Markus Sattler)
 * 
 * Install via: Tools → Manage Libraries
 * ============================================================================
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================
const char* ssid_primary = "EsDehidrasiHouse";
const char* password_primary = "11011011";

const char* ssid_backup1 = "EsDehidrasi";
const char* password_backup1 = "11011011";

const char* ssid_backup2 = "ICT-LAB WORKSPACE";
const char* password_backup2 = "ICTLAB2024";

const int WIFI_RETRY_DELAY = 500;
const int WIFI_MAX_RETRY = 10;

// ============================================================================
// MQTT CONFIGURATION
// ============================================================================
const char* mqtt_server = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Device02";
const char* mqtt_pass = "Device02";
const char* mqtt_topic_camera_ip = "smartTrain/camera/ip";

const unsigned long IP_PUBLISH_INTERVAL = 10000;  // 10 seconds
unsigned long lastIPPublish = 0;

// ============================================================================
// WEBSOCKET CONFIGURATION
// ============================================================================
const int WS_PORT = 81;  // WebSocket port (different from HTTP 80)
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);

bool isStreaming = false;
uint8_t streamingClient = 255;  // No client streaming initially

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

unsigned long frameCount = 0;
unsigned long lastFrameTime = 0;

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
// WIFI CONNECTION (Multi-Credentials) - FIXED VERSION
// ============================================================================
void connectWiFiMulti() {
    Serial.println("📶 Connecting to WiFi...");
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // Disconnect from any previous connection
    delay(1000);
    
    // Try primary WiFi
    Serial.print("Trying: ");
    Serial.println(ssid_primary);
    WiFi.begin(ssid_primary, password_primary);
    
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_MAX_RETRY) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retryCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("📍 SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("📍 IP Address: ");
        Serial.println(WiFi.localIP());
        return;
    }
    
    // Disconnect and try backup1
    Serial.println("\n⚠️  Primary WiFi failed, trying backup1...");
    WiFi.disconnect(true);  // IMPORTANT: Disconnect before retry!
    delay(1000);
    
    Serial.print("Trying: ");
    Serial.println(ssid_backup1);
    WiFi.begin(ssid_backup1, password_backup1);
    
    retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_MAX_RETRY) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retryCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected (backup1)!");
        Serial.print("📍 SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("📍 IP Address: ");
        Serial.println(WiFi.localIP());
        return;
    }
    
    // Disconnect and try backup2
    Serial.println("\n⚠️  Backup1 WiFi failed, trying backup2...");
    WiFi.disconnect(true);  // IMPORTANT: Disconnect before retry!
    delay(1000);
    
    Serial.print("Trying: ");
    Serial.println(ssid_backup2);
    WiFi.begin(ssid_backup2, password_backup2);
    
    retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < WIFI_MAX_RETRY) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retryCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected (backup2)!");
        Serial.print("📍 SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("📍 IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ All WiFi attempts failed!");
    }
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================
void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("🔌 Connecting to MQTT... ");
        
        if (mqttClient.connect("ESP32-CAM-WebSocket", mqtt_user, mqtt_pass)) {
            Serial.println("✅ Connected!");
            publishIP();  // Publish IP on connect
        } else {
            Serial.print("❌ Failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5s...");
            delay(5000);
        }
    }
}

void publishIP() {
    if (mqttClient.connected()) {
        String ipMessage = "{\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        
        if (mqttClient.publish(mqtt_topic_camera_ip, ipMessage.c_str())) {
            Serial.println("📡 IP published to smartTrain/camera/ip");
            Serial.print("   IP: ");
            Serial.println(WiFi.localIP());
            lastIPPublish = millis();
        } else {
            Serial.println("⚠️  IP publish failed!");
        }
    }
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("🔌 Client #%u disconnected\n", num);
            if (num == streamingClient) {
                isStreaming = false;
                streamingClient = 255;
            }
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("✅ Client #%u connected from %d.%d.%d.%d\n", 
                         num, ip[0], ip[1], ip[2], ip[3]);
            
            // Send welcome message
            String welcomeMsg = "{\"status\":\"connected\",\"version\":\"3.0\"}";
            webSocket.sendTXT(num, welcomeMsg);
            break;
        }
            
        case WStype_TEXT: {
            String message = String((char*)payload);
            Serial.printf("📨 Message from #%u: %s\n", num, message.c_str());
            
            if (message == "GET_FRAME") {
                // Send single frame
                sendFrame(num);
            }
            else if (message == "START_STREAM") {
                // Start continuous streaming
                isStreaming = true;
                streamingClient = num;
                Serial.printf("▶️  Streaming started for client #%u\n", num);
            }
            else if (message == "STOP_STREAM") {
                // Stop streaming
                if (num == streamingClient) {
                    isStreaming = false;
                    streamingClient = 255;
                    Serial.printf("⏸️  Streaming stopped for client #%u\n", num);
                }
            }
            else if (message == "GET_STATUS") {
                // Send camera status
                sendStatus(num);
            }
            break;
        }
            
        case WStype_BIN:
            // Binary data received (not used in this application)
            Serial.printf("📦 Binary data from #%u, length: %u\n", num, length);
            break;
    }
}

// ============================================================================
// SEND FRAME VIA WEBSOCKET
// ============================================================================
void sendFrame(uint8_t clientNum) {
    camera_fb_t * fb = esp_camera_fb_get();
    
    if (!fb) {
        Serial.println("❌ Camera capture failed");
        return;
    }
    
    // Send JPEG as binary data
    webSocket.sendBIN(clientNum, fb->buf, fb->len);
    
    frameCount++;
    lastFrameTime = millis();
    
    esp_camera_fb_return(fb);
}

// ============================================================================
// SEND STATUS VIA WEBSOCKET
// ============================================================================
void sendStatus(uint8_t clientNum) {
    String status = "{";
    status += "\"frames\":" + String(frameCount) + ",";
    status += "\"uptime\":" + String(millis() / 1000) + ",";
    status += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    status += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    status += "\"streaming\":" + String(isStreaming ? "true" : "false");
    status += "}";
    
    webSocket.sendTXT(clientNum, status);
}

// ============================================================================
// HTTP HANDLERS (Backup endpoints)
// ============================================================================
static esp_err_t index_handler(httpd_req_t *req) {
    const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM WebSocket</title>
    <style>
        body { font-family: Arial; text-align: center; margin: 50px; }
        h1 { color: #333; }
        .info { background: #f0f0f0; padding: 20px; border-radius: 10px; }
    </style>
</head>
<body>
    <h1>ESP32-CAM WebSocket Server</h1>
    <div class="info">
        <h2>WebSocket Endpoint</h2>
        <p><strong>ws://%s:%d</strong></p>
        <h3>Commands:</h3>
        <p>GET_FRAME - Get single frame</p>
        <p>START_STREAM - Start streaming</p>
        <p>STOP_STREAM - Stop streaming</p>
        <p>GET_STATUS - Get status</p>
    </div>
</body>
</html>
)rawliteral";

    char response[1024];
    snprintf(response, sizeof(response), html, WiFi.localIP().toString().c_str(), WS_PORT);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json[256];
    snprintf(json, sizeof(json),
             "{\"frames\":%lu,\"uptime\":%lu,\"heap\":%d,\"streaming\":%s}",
             frameCount, millis()/1000, ESP.getFreeHeap(), 
             isStreaming ? "true" : "false");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    
    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };
    
    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };
    
    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &index_uri);
        httpd_register_uri_handler(stream_httpd, &capture_uri);
        httpd_register_uri_handler(stream_httpd, &status_uri);
        Serial.println("✅ HTTP server started");
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n========================================");
    Serial.println("📷 SMART TRAIN ESP32-CAM MODULE");
    Serial.println("    WebSocket Real-time Streaming");
    Serial.println("    v3.0 WebSocket");
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
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;
    config.fb_count = 1;

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("❌ Camera init failed: 0x%x\n", err);
        delay(3000);
        ESP.restart();
        return;
    }
    Serial.println("✅ Camera initialized");
    Serial.println("   Frame size: QVGA (320x240)");

    // WiFi connection
    connectWiFiMulti();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi connection failed!");
        delay(3000);
        ESP.restart();
        return;
    }

    // Start HTTP server (backup)
    startCameraServer();

    // Setup MQTT
    espClient.setCACert(root_ca);
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setBufferSize(512);
    connectMQTT();

    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("✅ WebSocket server started on port %d\n", WS_PORT);

    Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.println("📋 WebSocket Endpoint:");
    Serial.printf("   ws://%s:%d\n", WiFi.localIP().toString().c_str(), WS_PORT);
    Serial.println("========================================");
    Serial.println("📋 HTTP Endpoints (Backup):");
    Serial.printf("   http://%s/         - Homepage\n", WiFi.localIP().toString().c_str());
    Serial.printf("   http://%s/capture  - Snapshot\n", WiFi.localIP().toString().c_str());
    Serial.printf("   http://%s/status   - Status\n", WiFi.localIP().toString().c_str());
    Serial.println("========================================");
    Serial.println("✅ SYSTEM READY!");
    Serial.println("========================================\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
    
    // Periodic IP publish
    if (millis() - lastIPPublish >= IP_PUBLISH_INTERVAL) {
        publishIP();
    }
    
    // Handle WebSocket
    webSocket.loop();
    
    // Send frame if streaming
    if (isStreaming && streamingClient != 255) {
        sendFrame(streamingClient);
        delay(50);  // ~20 FPS
    }
}
