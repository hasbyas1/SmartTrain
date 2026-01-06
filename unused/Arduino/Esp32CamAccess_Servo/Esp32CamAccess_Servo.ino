#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "EsDehidrasiHouse";
const char* password = "11011011";

// Camera pins for ESP32-CAM
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

// Servo control
#define SERVO_PIN         14
const int BARRIER_UP = 90;
const int BARRIER_DOWN = 0;

httpd_handle_t stream_httpd = NULL;
Servo barrierServo;
bool barrierDown = false;

// Minimal stream handler
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
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!jpeg_converted) {
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

        vTaskDelay(pdMS_TO_TICKS(100)); // Slower frame rate
    }

    if (fb) esp_camera_fb_return(fb);
    else if (_jpg_buf) free(_jpg_buf);
    return res;
}

// Capture handler
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// Simple control handler
static esp_err_t control_handler(httpd_req_t *req) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (strstr(query, "down")) {
            barrierServo.write(BARRIER_DOWN);
            barrierDown = true;
            httpd_resp_send(req, "DOWN", 4);
        } else if (strstr(query, "up")) {
            barrierServo.write(BARRIER_UP);
            barrierDown = false;
            httpd_resp_send(req, "UP", 2);
        } else {
            httpd_resp_send(req, "INVALID", 7);
        }
        return ESP_OK;
    }
    
    // Handle POST data
    if (req->method == HTTP_POST) {
        char buf[100];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret > 0) {
            buf[ret] = '\0';
            if (strstr(buf, "down")) {
                barrierServo.write(BARRIER_DOWN);
                barrierDown = true;
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, "{\"status\":\"DOWN\"}", 17);
            } else if (strstr(buf, "up")) {
                barrierServo.write(BARRIER_UP);
                barrierDown = false;
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, "{\"status\":\"UP\"}", 15);
            }
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
    return ESP_FAIL;
}

// Status handler
static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, barrierDown ? "{\"barrier\":\"DOWN\"}" : "{\"barrier\":\"UP\"}", 
                   barrierDown ? 18 : 16);
    return ESP_OK;
}

// Ultra minimal index
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, "<html><head><title>ESP32-CAM</title></head><body>", -1);
    httpd_resp_send_chunk(req, "<h1>ESP32-CAM + Servo</h1>", -1);
    httpd_resp_send_chunk(req, "<p><a href='/stream'>Stream</a> | <a href='/capture'>Capture</a></p>", -1);
    httpd_resp_send_chunk(req, "<p>Barrier: ", -1);
    httpd_resp_send_chunk(req, barrierDown ? "DOWN" : "UP", -1);
    httpd_resp_send_chunk(req, "</p>", -1);
    httpd_resp_send_chunk(req, "<p><a href='/control?cmd=up'>UP</a> | <a href='/control?cmd=down'>DOWN</a></p>", -1);
    httpd_resp_send_chunk(req, "</body></html>", -1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.task_priority = 5;
    config.stack_size = 3072;  // Even smaller stack
    config.max_uri_handlers = 6;
    config.max_open_sockets = 3;  // Limit connections

    if (httpd_start(&stream_httpd, &config) != ESP_OK) {
        Serial.println("Failed to start server");
        return;
    }

    httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler},
        {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler},
        {.uri = "/control", .method = HTTP_GET, .handler = control_handler},
        {.uri = "/control", .method = HTTP_POST, .handler = control_handler},
        {.uri = "/status", .method = HTTP_GET, .handler = status_handler}
    };

    for (int i = 0; i < 6; i++) {
        httpd_register_uri_handler(stream_httpd, &uris[i]);
    }
    
    Serial.println("Server started");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP32-CAM with Servo...");

    // Initialize servo first
    barrierServo.attach(SERVO_PIN);
    barrierServo.write(BARRIER_UP);
    barrierDown = false;
    delay(500);

    // Camera config - use smaller frame size
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
    config.frame_size = FRAMESIZE_QVGA;  // Start small 320x240
    config.jpeg_quality = 15;  // Lower quality = less memory
    config.fb_count = 1;  // Single buffer

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return;
    }

    // WiFi connection
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    startCameraServer();

    Serial.print("Ready! Go to: http://");
    Serial.println(WiFi.localIP());
    
    // Print memory info
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    delay(30000);
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}