#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// WiFi credentials (same as ESP32-CAM)
const char* ssid = "ICT-LAB WORKSPACE";
const char* password = "ICTLAB2024";

// Hardware pins
const int BUZZER_PIN = 4;  // Optional


// Global objects
WebServer server(80);
Servo brakeServo;

// State variables
bool brakeDown = false;
unsigned long lastCommandTime = 0;
String lastCommand = "";
float lastConfidence = 0.0;
unsigned long totalCommands = 0;
unsigned long systemStartTime = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\nESP32 Train Brake Controller");
    Serial.println("==========================================");
    
    systemStartTime = millis();
    
    // Initialize hardware
    setupHardware();
    
    // Connect to WiFi
    connectToWiFi();
    
    // Setup web server routes
    setupWebServer();
    
    // Start server
    server.begin();
    Serial.println("HTTP Server started on port 80");
    
    printSystemInfo();
}

void setupHardware() {
    Serial.println("Initializing hardware...");
    
    // Servo initialization
    brakeServo.attach(SERVO_PIN);
    brakeServo.write(BRAKE_UP);  // Start with brake up
    delay(500); // Give servo time to move
    
    // Buzzer setup (optional)
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Test hardware
    Serial.println("Testing hardware...");
    
    // Test servo
    brakeServo.write(BRAKE_DOWN);
    delay(500);
    brakeServo.write(BRAKE_UP);
    delay(500);
    
    // Test buzzer (if connected)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    
    Serial.println("Hardware initialized and tested");
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);  // Disable power saving for better performance
    
    Serial.print("Connecting");
    int attempts = 0;
    
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
        // Success sound (if buzzer connected)
        soundAlert(2);
    } else {
        Serial.println();
        Serial.println("WiFi connection failed!");
        Serial.println("Please check SSID and password");
        
        // Error sound (if buzzer connected)
        for(int i = 0; i < 5; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
        }
    }
}

void setupWebServer() {
    Serial.println("Setting up web server routes...");
    
    // CORS headers for all responses
    server.enableCORS(true);
    
    // Root page - Control interface
    server.on("/", HTTP_GET, handleRootPage);
    
    // Status endpoint (JSON)
    server.on("/status", HTTP_GET, handleStatusPage);
    
    // Control endpoint (POST) - Main communication from Python server
    server.on("/control", HTTP_POST, handleControlPost);
    
    // Control endpoint (GET) - For manual web interface
    server.on("/control", HTTP_GET, handleControlGet);
    
    // OPTIONS handler for CORS
    server.on("/control", HTTP_OPTIONS, handleCorsOptions);
    
    // Test endpoint
    server.on("/test", HTTP_GET, []() {
        server.send(200, "text/plain", "Train Brake Controller - Test OK");
    });
    
    Serial.println("Web server routes configured");
}

void handleRootPage() {
    String html = generateDashboardHTML();
    server.send(200, "text/html", html);
}

void handleStatusPage() {
    StaticJsonDocument<300> status;
    status["brake_down"] = brakeDown;
    status["brake_state"] = brakeDown ? "DOWN" : "UP";
    status["last_command"] = lastCommand;
    status["last_confidence"] = lastConfidence;
    status["total_commands"] = totalCommands;
    status["uptime_seconds"] = (millis() - systemStartTime) / 1000;
    status["wifi_rssi"] = WiFi.RSSI();
    status["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    status["free_heap"] = ESP.getFreeHeap();
    status["ip_address"] = WiFi.localIP().toString();
    status["servo_pin"] = SERVO_PIN;
    
    String response;
    serializeJson(status, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleControlPost() {
    if (!server.hasArg("plain")) {
        Serial.println("No data received in POST request");
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    String body = server.arg("plain");
    Serial.println("📡 Received POST data: " + body);
    
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String command = doc["command"] | "";
    String value = doc["value"] | "";
    float confidence = doc["confidence"] | 0.0;
    
    Serial.println("📡 Parsed command: " + command + " = " + value + 
                   " (confidence: " + String(confidence, 2) + ")");
    
    bool success = executeCommand(command, value, confidence, false);
    
    if (success) {
        StaticJsonDocument<200> response;
        response["status"] = "success";
        response["command"] = command;
        response["value"] = value;
        response["brake_state"] = brakeDown ? "DOWN" : "UP";
        response["timestamp"] = millis();
        response["confidence"] = confidence;
        
        String responseStr;
        serializeJson(response, responseStr);
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", responseStr);
        
        Serial.println("Command executed successfully");
    } else {
        Serial.println("Command execution failed");
        server.send(400, "application/json", "{\"error\":\"Invalid command\"}");
    }
}

void handleControlGet() {
    String command = server.arg("command");
    String value = server.arg("value");
    
    Serial.println("Manual web control: " + command + " = " + value);
    
    if (command.length() > 0 && value.length() > 0) {
        bool success = executeCommand(command, value, 0.0, true);
        
        String response = "<html><head><title>Control Result</title></head><body>";
        response += "<h1>ESP32 Train Controller</h1>";
        
        if (success) {
            response += "<h2 style='color: green;'>Command Executed Successfully!</h2>";
            response += "<p><strong>Command:</strong> " + command + " = " + value + "</p>";
            response += "<p><strong>Brake State:</strong> " + String(brakeDown ? "DOWN" : "UP") + "</p>";
        } else {
            response += "<h2 style='color: red;'>Command Failed!</h2>";
            response += "<p>Invalid command: " + command + " = " + value + "</p>";
        }
        
        response += "<p><a href='/'>Back to Dashboard</a></p>";
        response += "</body></html>";
        
        server.send(success ? 200 : 400, "text/html", response);
    } else {
        server.send(400, "text/html", 
            "<h1>Missing Parameters</h1>"
            "<p>Please provide both 'command' and 'value' parameters</p>"
            "<p><a href='/'>Back to Dashboard</a></p>");
    }
}

void handleCorsOptions() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
}

String generateDashboardHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Train Brake Controller</title>
    <meta http-equiv="refresh" content="5">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            margin: 0; 
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        .container { 
            max-width: 800px; 
            margin: 0 auto; 
            background: rgba(255,255,255,0.1); 
            padding: 30px; 
            border-radius: 20px; 
            backdrop-filter: blur(15px);
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }
        .header { text-align: center; margin-bottom: 30px; }
        .status-card { 
            background: rgba(255,255,255,0.2); 
            padding: 25px; 
            margin: 20px 0; 
            border-radius: 15px; 
            border-left: 6px solid;
        }
        .status-up { border-left-color: #28a745; }
        .status-down { border-left-color: #dc3545; }
        .btn { 
            display: inline-block;
            padding: 15px 30px; 
            margin: 10px; 
            border: none; 
            border-radius: 50px; 
            cursor: pointer; 
            font-size: 16px;
            font-weight: bold;
            text-decoration: none;
            transition: all 0.3s ease;
            text-align: center;
            min-width: 150px;
        }
        .btn-danger { 
            background: linear-gradient(45deg, #dc3545, #c82333);
            color: white; 
        }
        .btn-success { 
            background: linear-gradient(45deg, #28a745, #20c997);
            color: white; 
        }
        .btn:hover { 
            transform: translateY(-3px); 
            box-shadow: 0 6px 20px rgba(0,0,0,0.3); 
        }
        .stats { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); 
            gap: 20px; 
            margin: 30px 0; 
        }
        .stat-item { 
            background: rgba(255,255,255,0.2); 
            padding: 20px; 
            border-radius: 15px; 
            text-align: center;
        }
        .big-number { 
            font-size: 2.5em; 
            font-weight: bold; 
            color: #ffd700;
            display: block;
            margin-bottom: 10px;
        }
        .control-section { text-align: center; margin: 30px 0; }
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin: 20px 0;
        }
        .info-item {
            background: rgba(255,255,255,0.1);
            padding: 15px;
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Train Brake Controller</h1>
        </div>
        
        <div class="status-card )rawliteral";
    
    if (brakeDown) {
        html += "status-down";
    } else {
        html += "status-up";
    }
    
    html += R"rawliteral(">
            <h2>Brake Status: )rawliteral";
            
    if (brakeDown) {
        html += "DOWN (BRAKING)";
    } else {
        html += "UP (STANDBY)";
    }
    
    html += R"rawliteral(</h2>
            <div class="info-grid">
                <div class="info-item">
                    <strong>Last Command:</strong><br>)rawliteral";
    html += (lastCommand.length() > 0) ? lastCommand : "None";
    html += R"rawliteral(</div>
                <div class="info-item">
                    <strong>Confidence:</strong><br>)rawliteral";
    html += String(lastConfidence, 2) + "%";
    html += R"rawliteral(</div>
            </div>
        </div>
        
        <div class="stats">
            <div class="stat-item">
                <span class="big-number">)rawliteral";
    html += String(totalCommands);
    html += R"rawliteral(</span>
                <div>Total Commands</div>
            </div>
            <div class="stat-item">
                <span class="big-number">)rawliteral";
    html += String((millis() - systemStartTime) / 1000);
    html += R"rawliteral(</span>
                <div>Uptime (seconds)</div>
            </div>
            <div class="stat-item">
                <span class="big-number">)rawliteral";
    html += String(WiFi.RSSI());
    html += R"rawliteral(</span>
                <div>WiFi Signal (dBm)</div>
            </div>
        </div>
        
        <div class="status-card">
            <h3>Manual Control</h3>
            <p>Use these buttons for manual brake control:</p>
            <div class="control-section">
                <a href="/control?command=brake&value=down" class="btn btn-danger">Lower Brake</a>
                <a href="/control?command=brake&value=up" class="btn btn-success">Raise Brake</a>
            </div>
        </div>
        
        <div class="status-card">
            <h3>System Information</h3>
            <div class="info-grid">
                <div class="info-item">
                    <strong>IP Address:</strong><br>)rawliteral";
    html += WiFi.localIP().toString();
    html += R"rawliteral(</div>
                <div class="info-item">
                    <strong>WiFi Status:</strong><br>)rawliteral";
    html += (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected";
    html += R"rawliteral(</div>
                <div class="info-item">
                    <strong>Free Memory:</strong><br>)rawliteral";
    html += String(ESP.getFreeHeap() / 1024) + " KB";
    html += R"rawliteral(</div>
                <div class="info-item">
                    <strong>Servo Pin:</strong><br>GPIO )rawliteral";
    html += String(SERVO_PIN);
    html += R"rawliteral(</div>
            </div>
        </div>
        
        <div style="text-align: center; margin-top: 30px; opacity: 0.7;">
            <p>Auto-refresh every 5 seconds</p>
        </div>
    </div>
</body>
</html>
    )rawliteral";
    
    return html;
}

bool executeCommand(String command, String value, float confidence, bool manual) {
    Serial.print("Executing: ");
    Serial.print(command);
    Serial.print(" = ");
    Serial.print(value);
    Serial.println(manual ? " [MANUAL]" : " [AUTO]");
    
    if (command == "brake") {
        if (value == "down" || value == "DOWN") {
            lowerBrake(confidence, manual);
            return true;
        } else if (value == "up" || value == "UP") {
            raiseBrake(manual);
            return true;
        }
    }
    
    Serial.println("Unknown command: " + command);
    return false;
}

void lowerBrake(float confidence, bool manual) {
    if (!brakeDown) {
        Serial.print("LOWERING BRAKE");
        Serial.print(manual ? " [MANUAL]" : " [AUTO]");
        Serial.print(" - Confidence: ");
        Serial.print(confidence, 2);
        Serial.println("%");
        
        // Sound alert (2 beeps for lowering)
        soundAlert(2);
        
        // Move servo gradually for smoother operation
        for (int angle = BRAKE_UP; angle >= BRAKE_DOWN; angle -= 5) {
            brakeServo.write(angle);
            delay(50);
        }
        brakeServo.write(BRAKE_DOWN);
        
        brakeDown = true;
        
        // Update tracking
        lastCommandTime = millis();
        lastConfidence = confidence;
        totalCommands++;
        lastCommand = "brake=down";
        lastCommand += (manual ? " (manual)" : " (auto)");
        
        Serial.println("Brake lowered successfully");
    } else {
        Serial.println("Brake already down - ignoring command");
    }
}

void raiseBrake(bool manual) {
    if (brakeDown) {
        Serial.print("RAISING BRAKE");
        Serial.println(manual ? " [MANUAL]" : " [AUTO]");
        
        // Sound alert (1 beep for raising)
        soundAlert(1);
        
        // Move servo gradually for smoother operation
        for (int angle = BRAKE_DOWN; angle <= BRAKE_UP; angle += 5) {
            brakeServo.write(angle);
            delay(50);
        }
        brakeServo.write(BRAKE_UP);
        
        brakeDown = false;
        
        // Update tracking
        lastCommandTime = millis();
        lastConfidence = 0.0;
        totalCommands++;
        lastCommand = "brake=up";
        lastCommand += (manual ? " (manual)" : " (auto)");
        
        Serial.println("Brake raised successfully");
    } else {
        Serial.println("Brake already up - ignoring command");
    }
}

void soundAlert(int beeps) {
    for (int i = 0; i < beeps; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(150);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < beeps - 1) delay(150);
    }
}

void checkSafetyTimeout() {
    // Safety feature: Auto-raise brake if no command for 60 seconds
    if (brakeDown && (millis() - lastCommandTime) > 60000) {
        Serial.println("SAFETY TIMEOUT - Auto-raising brake after 60 seconds");
        raiseBrake(false);
        lastCommand += " (SAFETY_TIMEOUT)";
    }
}

void printSystemInfo() {
    Serial.println("\n==================================================");
    Serial.println("SYSTEM INFORMATION");
    Serial.println("==================================================");
    Serial.println("Network:");
    Serial.println("   WiFi Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected ✅" : "Disconnected ❌"));
    Serial.println("   IP Address: " + WiFi.localIP().toString());
    Serial.println("   Signal: " + String(WiFi.RSSI()) + " dBm");
    
    Serial.println("Hardware:");
    Serial.println("   Servo Pin: GPIO" + String(SERVO_PIN));
    Serial.println("   Buzzer Pin: GPIO" + String(BUZZER_PIN) + " (optional)");
    
    Serial.println("Web Interface:");
    Serial.println("   Dashboard: http://" + WiFi.localIP().toString() + "/");
    Serial.println("   Status API: http://" + WiFi.localIP().toString() + "/status");
    Serial.println("   Control API: http://" + WiFi.localIP().toString() + "/control");
    
    Serial.println("==================================================");
    Serial.println("System ready!");
    Serial.println();
}

void printPeriodicStatus() {
    static unsigned long lastStatusPrint = 0;
    
    if (millis() - lastStatusPrint > 30000) {  // Every 30 seconds
        Serial.print("STATUS: Brake=");
        Serial.print(brakeDown ? "DOWN" : "UP");
        Serial.print(" | Commands=");
        Serial.print(totalCommands);
        Serial.print(" | Uptime=");
        Serial.print((millis() - systemStartTime) / 1000);
        Serial.print("s | WiFi=");
        Serial.print(WiFi.RSSI());
        Serial.println("dBm");
        lastStatusPrint = millis();
    }
}

void loop() {
    // Handle HTTP requests
    server.handleClient();
    
    // Safety checks
    checkSafetyTimeout();
    
    // Periodic status reporting
    printPeriodicStatus();
    
    // WiFi reconnection check
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, attempting reconnection...");
        WiFi.reconnect();
        delay(5000);
    }
    
    // Small delay to prevent overwhelming the CPU
    delay(10);
}

/*
 * WIRING (SIMPLE VERSION):
 * 
 * ESP32 Pin  →  Component
 * GPIO 18    →  Servo Signal Wire (Orange/Yellow)
 * GPIO 5     →  Buzzer + (optional)
 * 5V/VIN     →  Servo Red Wire
 * GND        →  Servo Brown/Black Wire, Buzzer -
 * 
 * COMMANDS:
 * HTTP POST /control: {"command": "brake", "value": "down", "confidence": 0.85}
 * HTTP GET /control?command=brake&value=up
 * 
 * FEATURES:
 * - Servo control with smooth movement
 * - Safety timeout (auto-raise after 60s)
 * - WiFi auto-reconnection
 * - Web dashboard with manual controls
 * - JSON API for Python integration
 * - Audio feedback (if buzzer connected)
 */