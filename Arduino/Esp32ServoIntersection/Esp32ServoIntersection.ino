#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "ICT-LAB WORKSPACE";
const char* password = "ICTLAB2024";

// Hardware pins
const int SERVO_PIN = 14;

// Servo positions
const int BARRIER_UP = 90;
const int BARRIER_DOWN = 0;

// Global objects
WebServer server(80);
Servo barrierServo;

// State variables
bool barrierDown = false;
unsigned long totalCommands = 0;
String lastCommand = "";

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 Servo Controller");
    
    // Initialize servo
    barrierServo.attach(SERVO_PIN);
    barrierServo.write(BARRIER_UP);
    barrierDown = false;
    delay(500);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Setup web server routes
    server.enableCORS(true);
    
    // Root page
    server.on("/", HTTP_GET, handleRoot);
    
    // Control endpoints
    server.on("/control", HTTP_POST, handleControlPost);
    server.on("/control", HTTP_GET, handleControlGet);
    server.on("/control", HTTP_OPTIONS, handleCorsOptions);
    
    // Status endpoint
    server.on("/status", handleStatus);
    
    // Start server
    server.begin();
    Serial.println("HTTP Server started");
    Serial.print("Dashboard: http://");
    Serial.println(WiFi.localIP());
}

void handleRoot() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Crossing Barrier Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 0; 
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        .container { 
            max-width: 600px; 
            margin: 0 auto; 
            background: rgba(255,255,255,0.1); 
            padding: 30px; 
            border-radius: 20px; 
            backdrop-filter: blur(15px);
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }
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
            min-width: 120px;
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
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); 
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
            font-size: 2em; 
            font-weight: bold; 
            color: #ffd700;
            display: block;
            margin-bottom: 10px;
        }
        .control-section { text-align: center; margin: 30px 0; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Crossing Barrier Controller</h1>
        </div>

        <div class="status-card )";
    
    html += barrierDown ? "status-down" : "status-up";
    html += R"(">
            <h2>Barrier Status: )";
    html += barrierDown ? "DOWN (BLOCKING)" : "UP (CLEAR)";
    html += R"(</h2>
            <p><strong>Last Command:</strong> )";
    html += lastCommand.length() > 0 ? lastCommand : "None";
    html += R"(</p>
        </div>
        
        <div class="stats">
            <div class="stat-item">
                <span class="big-number">)";
    html += String(totalCommands);
    html += R"(</span>
                <div>Total Commands</div>
            </div>
            <div class="stat-item">
                <span class="big-number">)";
    html += String(millis() / 1000);
    html += R"(</span>
                <div>Uptime (sec)</div>
            </div>
        </div>
        
        <div class="control-section">
            <h3>Manual Control</h3>
            <a href="/control?command=barrier&value=down" class="btn btn-danger">Lower Barrier</a>
            <a href="/control?command=barrier&value=up" class="btn btn-success">Raise Barrier</a>
        </div>
        
        <div class="status-card">
            <h3>API Information</h3>
            <p><strong>Status:</strong> <a href="/status" style="color: #ffd700;">/status</a></p>
            <p><strong>Control:</strong> POST /control with JSON</p>
            <p><strong>Example:</strong> {"command": "barrier", "value": "up"}</p>
        </div>
    </div>
</body>
</html>
    )";
    
    server.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<200> doc;
  doc["barrier"] = barrierDown ? "DOWN" : "UP";  // variabel barrierDown sudah ada di kode kamu
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

void handleControlPost() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    String body = server.arg("plain");
    Serial.println("Received POST: " + body);
    
    // Simple JSON parsing
    String command = "";
    String value = "";
    
    int cmdStart = body.indexOf("\"command\"");
    if (cmdStart > -1) {
        cmdStart = body.indexOf(":", cmdStart) + 1;
        cmdStart = body.indexOf("\"", cmdStart) + 1;
        int cmdEnd = body.indexOf("\"", cmdStart);
        command = body.substring(cmdStart, cmdEnd);
    }
    
    int valStart = body.indexOf("\"value\"");
    if (valStart > -1) {
        valStart = body.indexOf(":", valStart) + 1;
        valStart = body.indexOf("\"", valStart) + 1;
        int valEnd = body.indexOf("\"", valStart);
        value = body.substring(valStart, valEnd);
    }
    
    Serial.println("Command: " + command + ", Value: " + value);
    
    if (command == "barrier") {
        if (value == "down") {
            barrierServo.write(BARRIER_DOWN);
            barrierDown = true;
            lastCommand = "barrier=down (API)";
            totalCommands++;
        } else if (value == "up") {
            barrierServo.write(BARRIER_UP);
            barrierDown = false;
            lastCommand = "barrier=up (API)";
            totalCommands++;
        }
        
        String response = "{";
        response += "\"status\":\"success\",";
        response += "\"command\":\"" + command + "\",";
        response += "\"value\":\"" + value + "\",";
        response += "\"barrier_state\":\"" + String(barrierDown ? "DOWN" : "UP") + "\"";
        response += "}";
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", response);
    } else {
        server.send(400, "application/json", "{\"error\":\"Invalid command\"}");
    }
}

void handleControlGet() {
    String command = server.arg("command");
    String value = server.arg("value");
    
    Serial.println("Manual control: " + command + " = " + value);
    
    if (command == "barrier") {
        if (value == "down") {
            barrierServo.write(BARRIER_DOWN);
            barrierDown = true;
            lastCommand = "barrier=down (manual)";
            totalCommands++;
        } else if (value == "up") {
            barrierServo.write(BARRIER_UP);
            barrierDown = false;
            lastCommand = "barrier=up (manual)";
            totalCommands++;
        }
        
        String html = "<!DOCTYPE html><html><head><title>Command Result</title></head><body>";
        html += "<h1>ESP32 Servo Controller</h1>";
        html += "<h2 style='color: green;'>Command Executed!</h2>";
        html += "<p><strong>Command:</strong> " + command + " = " + value + "</p>";
        html += "<p><strong>Barrier State:</strong> " + String(barrierDown ? "DOWN" : "UP") + "</p>";
        html += "<p><a href='/'>Back to Dashboard</a></p>";
        html += "</body></html>";
        
        server.send(200, "text/html", html);
    } else {
        server.send(400, "text/html", "<h1>Invalid Command</h1><p><a href='/'>Back</a></p>");
    }
}

void handleCorsOptions() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
}

void loop() {
    server.handleClient();
    delay(2);
}