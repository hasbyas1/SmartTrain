import cv2
import numpy as np
import requests
from ultralytics import YOLO
import time
import threading
from flask import Flask, render_template_string
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json
import os
import ssl

class SmartCrossingDetector:
    def __init__(self, esp32_cam_ip, model_path, mqtt_broker, mqtt_port, mqtt_user, mqtt_pass, mqtt_topic):
        """
        Smart Train Level Crossing - Simplified Version
        Hanya 1 IP untuk ESP32-CAM + Servo Palang
        """
        # Device configuration
        self.esp32_cam_ip = esp32_cam_ip
        self.capture_url = f"http://{esp32_cam_ip}/capture"
        self.stream_url = f"http://{esp32_cam_ip}/stream"
        
        # MQTT Configuration
        self.mqtt_broker = mqtt_broker
        self.mqtt_port = mqtt_port
        self.mqtt_user = mqtt_user
        self.mqtt_pass = mqtt_pass
        self.mqtt_topic = mqtt_topic
        
        # ML Model
        self.model = YOLO(model_path)
        self.conf_threshold = 0.6
        
        # Detection state
        self.current_detections = {"bus": False, "car": False}
        self.barrier_state = "UP"  # UP or DOWN
        
        # Performance tracking
        self.total_frames_processed = 0
        self.detection_count = 0
        self.last_detection_time = 0
        
        # Flask + SocketIO setup
        self.app = Flask(__name__)
        self.app.config['SECRET_KEY'] = 'smart_crossing_secret'
        self.socketio = SocketIO(self.app, cors_allowed_origins="*")
        
        # Detection control
        self.detection_running = False
        self.detection_thread = None
        
        # MQTT Client
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.username_pw_set(mqtt_user, mqtt_pass)
        self.mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS)
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        self.setup_routes()
        self.setup_socketio_handlers()
        
        print(f"Smart Crossing Detector Initialized")
        print(f"ESP32-CAM: {esp32_cam_ip}")
        print(f"MQTT Broker: {mqtt_broker}")
        print(f"Model: {model_path}")
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print("✅ Connected to MQTT Broker")
            # Subscribe untuk menerima status dari ESP32
            client.subscribe(self.mqtt_topic)
        else:
            print(f"❌ Failed to connect, return code {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        """MQTT message received callback"""
        try:
            payload = json.loads(msg.payload.decode())
            if "status" in payload:
                status = payload["status"]
                print(f"📩 MQTT Received: {status}")
                # Update internal state
                if status == "Tertutup":
                    self.barrier_state = "DOWN"
                elif status == "Terbuka":
                    self.barrier_state = "UP"
        except Exception as e:
            print(f"MQTT message error: {e}")
    
    def connect_mqtt(self):
        """Connect to MQTT broker"""
        try:
            print(f"Connecting to MQTT Broker: {self.mqtt_broker}:{self.mqtt_port}")
            self.mqtt_client.connect(self.mqtt_broker, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            return True
        except Exception as e:
            print(f"❌ MQTT connection failed: {e}")
            return False
    
    def send_barrier_command(self, command):
        """
        Send barrier command via MQTT
        command: "Terbuka" atau "Tertutup"
        """
        try:
            payload = json.dumps({"status": command})
            result = self.mqtt_client.publish(self.mqtt_topic, payload)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"📤 MQTT Published: {payload}")
                self.barrier_state = "UP" if command == "Terbuka" else "DOWN"
                return True
            else:
                print(f"❌ MQTT Publish failed: {result.rc}")
                return False
        except Exception as e:
            print(f"❌ MQTT error: {e}")
            return False
    
    def setup_routes(self):
        """Setup Flask HTTP routes"""
        
        @self.app.route('/')
        def dashboard():
            """Main monitoring dashboard"""
            return render_template_string("""
<!DOCTYPE html>
<html>
<head>
    <title>Smart Railway Crossing</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.1/socket.io.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; }
        .status-panel { background: white; padding: 20px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .status-up { background-color: #d4edda; border-left: 5px solid #28a745; }
        .status-down { background-color: #f8d7da; border-left: 5px solid #dc3545; }
        .control-buttons button { margin: 5px; padding: 10px 20px; font-size: 16px; border: none; border-radius: 4px; cursor: pointer; }
        .btn-danger { background: #dc3545; color: white; }
        .btn-success { background: #28a745; color: white; }
        .btn-primary { background: #007bff; color: white; }
        .video-container { text-align: center; margin: 20px 0; }
        #camera-feed { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 4px; }
        .stats { display: flex; justify-content: space-around; text-align: center; }
        .stat-item { background: white; padding: 15px; border-radius: 8px; margin: 5px; flex: 1; }
        h1 { color: #333; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚂 Smart Railway Crossing Detector</h1>
        
        <!-- Status Panel -->
        <div class="status-panel" id="status-panel">
            <h2>Barrier Status: <span id="barrier-status">UNKNOWN</span></h2>
            <p><strong>Last Detection:</strong> <span id="last-detection">None</span></p>
            <p><strong>Detection Count:</strong> <span id="detection-count">0</span></p>
            <p><strong>System Status:</strong> <span id="system-status">Connecting...</span></p>
        </div>
        
        <!-- Live Camera Feed -->
        <div class="video-container">
            <h3>📷 Live Camera Feed</h3>
            <img id="camera-feed" src="{{ stream_url }}" alt="ESP32-CAM Stream">
        </div>
        
        <!-- Control Panel -->
        <div class="status-panel">
            <h3>Control Panel</h3>
            <div class="control-buttons">
                <button class="btn-primary" onclick="startDetection()">▶️ Start Detection</button>
                <button class="btn-danger" onclick="stopDetection()">⏹️ Stop Detection</button>
                <button class="btn-success" onclick="controlBarrier('Terbuka')">⬆️ Open Barrier</button>
                <button class="btn-danger" onclick="controlBarrier('Tertutup')">⬇️ Close Barrier</button>
            </div>
        </div>
        
        <!-- Statistics -->
        <div class="stats">
            <div class="stat-item">
                <h4>Total Frames</h4>
                <span id="total-frames">0</span>
            </div>
            <div class="stat-item">
                <h4>Vehicle Detections</h4>
                <span id="vehicle-detections">0</span>
            </div>
            <div class="stat-item">
                <h4>System Uptime</h4>
                <span id="uptime">0s</span>
            </div>
        </div>
    </div>

    <script>
        const socket = io();
        let startTime = Date.now();
        
        // Update uptime
        setInterval(() => {
            const uptime = Math.floor((Date.now() - startTime) / 1000);
            document.getElementById('uptime').textContent = uptime + 's';
        }, 1000);
        
        // Socket event handlers
        socket.on('connect', function() {
            document.getElementById('system-status').textContent = 'Connected';
            console.log('Connected to server');
        });
        
        socket.on('detection_update', function(data) {
            document.getElementById('last-detection').textContent = 
                data.bus_detected ? 'Bus/Car detected!' : 'No vehicle';
            updateBarrierDisplay(data.barrier_state);
        });
        
        socket.on('system_stats', function(data) {
            document.getElementById('total-frames').textContent = data.total_frames;
            document.getElementById('vehicle-detections').textContent = data.detection_count;
            document.getElementById('detection-count').textContent = data.detection_count;
        });
        
        // Control functions
        function controlBarrier(action) {
            fetch('/api/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: action})
            })
            .then(response => response.json())
            .then(data => console.log('Command sent:', data))
            .catch(error => console.error('Error:', error));
        }
        
        function startDetection() {
            fetch('/api/detection/start', {method: 'POST'})
            .then(response => response.json())
            .then(data => console.log('Detection started:', data));
        }
        
        function stopDetection() {
            fetch('/api/detection/stop', {method: 'POST'})
            .then(response => response.json())
            .then(data => console.log('Detection stopped:', data));
        }
        
        function updateBarrierDisplay(state) {
            const statusPanel = document.getElementById('status-panel');
            const statusText = document.getElementById('barrier-status');
            
            if (state === 'DOWN') {
                statusPanel.className = 'status-panel status-down';
                statusText.textContent = 'DOWN ⬇️';
            } else {
                statusPanel.className = 'status-panel status-up';
                statusText.textContent = 'UP ⬆️';
            }
        }
    </script>
</body>
</html>
            """, stream_url=self.stream_url)
        
        @self.app.route('/api/control', methods=['POST'])
        def control():
            """Manual barrier control"""
            data = request.get_json()
            command = data.get('command', '')
            
            if command in ['Terbuka', 'Tertutup']:
                success = self.send_barrier_command(command)
                return jsonify({'status': 'success' if success else 'failed', 'command': command})
            return jsonify({'status': 'error', 'message': 'Invalid command'})
        
        @self.app.route('/api/detection/start', methods=['POST'])
        def start_detection():
            """Start detection loop"""
            self.start_detection_loop()
            return jsonify({'status': 'started'})
        
        @self.app.route('/api/detection/stop', methods=['POST'])
        def stop_detection():
            """Stop detection loop"""
            self.stop_detection_loop()
            return jsonify({'status': 'stopped'})
        
        from flask import request, jsonify
    
    def setup_socketio_handlers(self):
        """Setup SocketIO handlers"""
        
        @self.socketio.on('connect')
        def handle_connect():
            print('Client connected')
        
        @self.socketio.on('disconnect')
        def handle_disconnect():
            print('Client disconnected')
    
    def test_camera_connection(self):
        """Test ESP32-CAM connection"""
        try:
            response = requests.get(self.capture_url, timeout=5)
            if response.status_code == 200:
                print(f"✅ ESP32-CAM connected: {self.esp32_cam_ip}")
                return True
            else:
                print(f"❌ ESP32-CAM responded with status: {response.status_code}")
                return False
        except Exception as e:
            print(f"❌ ESP32-CAM not reachable: {e}")
            return False
    
    def capture_frame_from_camera(self):
        """Capture frame from ESP32-CAM"""
        try:
            response = requests.get(self.capture_url, timeout=5)
            if response.status_code == 200:
                img_array = np.frombuffer(response.content, np.uint8)
                frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                return frame
            else:
                print(f"Failed to capture: {response.status_code}")
                return None
        except Exception as e:
            print(f"Capture error: {e}")
            return None
    
    def detect_objects(self, frame):
        """
        Detect bus and car only using YOLO
        Returns: detections dict, max_confidence, annotated_frame
        """
        try:
            # Run YOLO detection
            results = self.model(frame, conf=self.conf_threshold, verbose=False)
            
            detections = {"bus": False, "car": False}
            max_confidence = 0.0
            
            # Process results
            annotated_frame = frame.copy()
            
            for result in results:
                boxes = result.boxes
                for box in boxes:
                    cls = int(box.cls[0])
                    conf = float(box.conf[0])
                    class_name = self.model.names[cls]
                    
                    # Only detect bus and car
                    if class_name in ['bus', 'car']:
                        detections[class_name] = True
                        max_confidence = max(max_confidence, conf)
                        
                        # Draw bounding box
                        x1, y1, x2, y2 = map(int, box.xyxy[0])
                        color = (0, 255, 0) if class_name == 'bus' else (255, 0, 0)
                        cv2.rectangle(annotated_frame, (x1, y1), (x2, y2), color, 2)
                        
                        # Draw label
                        label = f"{class_name}: {conf:.2f}"
                        cv2.putText(annotated_frame, label, (x1, y1 - 10),
                                  cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
            
            return detections, max_confidence, annotated_frame
            
        except Exception as e:
            print(f"Detection error: {e}")
            return {"bus": False, "car": False}, 0.0, frame
    
    def process_detection_logic(self, detections, confidence):
        """
        Process detection and control barrier via MQTT
        Logic: Jika ada bus/car → turunkan palang, jika tidak → naikkan palang
        """
        current_time = time.time()
        action_taken = None
        
        # Logika sederhana: ada kendaraan → turunkan, tidak ada → naikkan
        if detections["bus"] or detections["car"]:
            # Ada kendaraan terdeteksi
            if self.barrier_state != "DOWN":
                if self.send_barrier_command("Tertutup"):
                    action_taken = "BARRIER_LOWERED"
                    self.detection_count += 1
                    print(f"🚗 Vehicle detected! Lowering barrier (confidence: {confidence:.2f})")
        else:
            # Tidak ada kendaraan
            if self.barrier_state != "UP":
                if self.send_barrier_command("Terbuka"):
                    action_taken = "BARRIER_RAISED"
                    print(f"✅ No vehicle detected. Raising barrier")
        
        # Broadcast detection via WebSocket
        self.socketio.emit('detection_update', {
            'bus_detected': detections["bus"] or detections["car"],
            'confidence': confidence,
            'timestamp': current_time,
            'action': action_taken,
            'barrier_state': self.barrier_state
        })
        
        # Update stats
        self.socketio.emit('system_stats', {
            'total_frames': self.total_frames_processed,
            'detection_count': self.detection_count
        })
        
        self.current_detections = detections
        if detections["bus"] or detections["car"]:
            self.last_detection_time = current_time
    
    def detection_loop(self):
        """Main detection loop running in separate thread"""
        print("🚀 Starting detection loop...")
        
        # Setup OpenCV window
        window_name = "Smart Railway Crossing - Live Detection"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 800, 600)
        
        while self.detection_running:
            try:
                # Capture frame
                frame = self.capture_frame_from_camera()
                if frame is not None:
                    self.total_frames_processed += 1
                    
                    # Run detection and get annotated frame
                    detections, confidence, annotated_frame = self.detect_objects(frame)
                    
                    # Process detection logic
                    self.process_detection_logic(detections, confidence)
                    
                    # Display annotated frame
                    cv2.imshow(window_name, annotated_frame)
                    
                    # Check for quit key
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q'):
                        print("'q' pressed - stopping detection")
                        self.detection_running = False
                        break
                    elif key == ord('s'):
                        # Save screenshot
                        timestamp = int(time.time())
                        filename = f"detection_{timestamp}.jpg"
                        cv2.imwrite(filename, annotated_frame)
                        print(f"📸 Screenshot saved: {filename}")
                
                time.sleep(0.1)  # 10 FPS
                
            except Exception as e:
                print(f"Detection loop error: {e}")
                time.sleep(1)
        
        # Cleanup
        cv2.destroyAllWindows()
        print("Detection loop stopped")
    
    def start_detection_loop(self):
        """Start detection in separate thread"""
        if not self.detection_running:
            self.detection_running = True
            self.detection_thread = threading.Thread(target=self.detection_loop, daemon=True)
            self.detection_thread.start()
            print("✅ Detection started")
    
    def stop_detection_loop(self):
        """Stop detection loop"""
        self.detection_running = False
        if self.detection_thread:
            self.detection_thread.join(timeout=2)
        print("⏹️ Detection stopped")
    
    def run_server(self, host='0.0.0.0', port=5000, debug=False):
        """Run the Flask-SocketIO server"""
        print(f"\n🌐 Starting Smart Crossing Server on http://{host}:{port}")
        print(f"📊 Dashboard: http://{host}:{port}")
        
        self.socketio.run(self.app, host=host, port=port, debug=debug, allow_unsafe_werkzeug=True)

def main():
    # ==================== CONFIGURATION ====================
    # ESP32-CAM IP Address (satu-satunya device)
    ESP32_CAM_IP = "192.168.1.31"  # Ganti dengan IP ESP32-CAM kamu
    
    # YOLO Model Path
    MODEL_PATH = "./runs/detect/train/weights/best.pt"  # Path ke model YOLO kamu
    
    # MQTT Configuration (HiveMQ Cloud)
    MQTT_BROKER = "9e108cb03c734f0394b0f0b49508ec1e.s1.eu.hivemq.cloud"
    MQTT_PORT = 8883
    MQTT_USER = "Device02"
    MQTT_PASS = "Device02"
    MQTT_TOPIC = "smarttrain/palang"
    # =======================================================
    
    # Verify model exists
    if not os.path.exists(MODEL_PATH):
        print(f"❌ Model file not found: {MODEL_PATH}")
        print("Please check the path to your YOLO model file")
        return
    
    # Create detector
    detector = SmartCrossingDetector(
        esp32_cam_ip=ESP32_CAM_IP,
        model_path=MODEL_PATH,
        mqtt_broker=MQTT_BROKER,
        mqtt_port=MQTT_PORT,
        mqtt_user=MQTT_USER,
        mqtt_pass=MQTT_PASS,
        mqtt_topic=MQTT_TOPIC
    )
    
    # Test camera connection
    cam_ok = detector.test_camera_connection()
    if not cam_ok:
        print("⚠️ ESP32-CAM not connected - check IP address")
        print(f"Make sure ESP32-CAM is accessible at: {ESP32_CAM_IP}")
        response = input("Continue anyway? (y/n): ")
        if response.lower() != 'y':
            return
    
    # Connect to MQTT
    mqtt_ok = detector.connect_mqtt()
    if not mqtt_ok:
        print("⚠️ MQTT connection failed - check broker settings")
        response = input("Continue anyway? (y/n): ")
        if response.lower() != 'y':
            return
    
    # Start detection automatically if camera is available
    if cam_ok:
        detector.start_detection_loop()
    
    # Run server
    try:
        detector.run_server(port=5000, debug=False)
    except KeyboardInterrupt:
        print("\n⏹️ Shutting down server...")
        detector.stop_detection_loop()

if __name__ == "__main__":
    main()