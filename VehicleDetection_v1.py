import cv2
import numpy as np
import requests
from ultralytics import YOLO
import time
import threading
from queue import Queue
from flask import Flask, request, jsonify, render_template_string
from flask_socketio import SocketIO, emit
import json
from datetime import datetime
import os

class SmartTrainServer:
    def __init__(self, esp32_cam_ip, esp32_train_ip, model_path):
        """
        Smart Train Level Crossing Server
        Combines ML detection, HTTP communication, and WebSocket monitoring
        """
        # Device configurations
        self.esp32_cam_ip = esp32_cam_ip
        self.esp32_train_ip = esp32_train_ip
        self.capture_url = f"http://{esp32_cam_ip}/capture"
        self.train_control_url = f"http://{esp32_train_ip}/control"
        
        # ML Model
        self.model = YOLO(model_path)
        self.conf_threshold = 0.6
        
        # Detection state
        self.current_detections = {"bus": False, "car": False}
        self.detection_history = []
        self.consecutive_detections_needed = 3
        self.last_command_sent = None
        self.barrier_state = "UP"  # UP or DOWN
        
        # Performance tracking
        self.total_frames_processed = 0
        self.detection_count = 0
        self.last_detection_time = 0
        
        # Flask + SocketIO setup
        self.app = Flask(__name__)
        self.app.config['SECRET_KEY'] = 'smart_train_secret'
        self.socketio = SocketIO(self.app, cors_allowed_origins="*")
        
        # Detection control
        self.detection_running = False
        self.detection_thread = None
        
        self.setup_routes()
        self.setup_socketio_handlers()
        
        print(f"🚂 Smart Train Server Initialized")
        print(f"📷 ESP32-CAM: {esp32_cam_ip}")
        print(f"🎛️  ESP32-Train: {esp32_train_ip}")
        print(f"🧠 Model: {model_path}")
    
    def setup_routes(self):
        """Setup Flask HTTP routes"""
        
        @self.app.route('/')
        def dashboard():
            """Main monitoring dashboard"""
            return render_template_string("""
<!DOCTYPE html>
<html>
<head>
    <title>Smart Train Level Crossing</title>
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
        .detection-log { height: 300px; overflow-y: scroll; background: #f8f9fa; padding: 10px; border-radius: 4px; }
        .detection-item { margin: 5px 0; padding: 5px; background: white; border-radius: 4px; }
        .bus-detected { background: #fff3cd; border-left: 3px solid #ffc107; }
        .video-container { text-align: center; margin: 20px 0; }
        #camera-feed { max-width: 100%; height: auto; border: 2px solid #ddd; border-radius: 4px; }
        .stats { display: flex; justify-content: space-around; text-align: center; }
        .stat-item { background: white; padding: 15px; border-radius: 8px; margin: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚂 Smart Train Level Crossing Control</h1>
        
        <!-- Status Panel -->
        <div class="status-panel" id="status-panel">
            <h2>🚧 Barrier Status: <span id="barrier-status">UNKNOWN</span></h2>
            <p><strong>Last Detection:</strong> <span id="last-detection">None</span></p>
            <p><strong>Detection Count:</strong> <span id="detection-count">0</span></p>
            <p><strong>System Status:</strong> <span id="system-status">Connecting...</span></p>
        </div>
        
        <!-- Live Camera Feed -->
        <div class="video-container">
            <h3>📷 Live Camera Feed</h3>
            <img id="camera-feed" src="http://{{ esp32_cam_ip }}/stream" alt="ESP32-CAM Stream">
        </div>
        
        <!-- Control Panel -->
        <div class="status-panel">
            <h3>🎛️ Manual Control</h3>
            <div class="control-buttons">
                <button class="btn-danger" onclick="controlBarrier('down')">⬇️ Lower Barrier</button>
                <button class="btn-success" onclick="controlBarrier('up')">⬆️ Raise Barrier</button>
                <button class="btn-primary" onclick="startDetection()">▶️ Start Detection</button>
                <button class="btn-danger" onclick="stopDetection()">⏹️ Stop Detection</button>
            </div>
        </div>
        
        <!-- Statistics -->
        <div class="stats">
            <div class="stat-item">
                <h4>Total Frames</h4>
                <span id="total-frames">0</span>
            </div>
            <div class="stat-item">
                <h4>Bus Detections</h4>
                <span id="bus-detections">0</span>
            </div>
            <div class="stat-item">
                <h4>System Uptime</h4>
                <span id="uptime">0s</span>
            </div>
        </div>
        
        <!-- Detection Log -->
        <div class="status-panel">
            <h3>📋 Detection Log</h3>
            <div class="detection-log" id="detection-log">
                <!-- Detection entries will appear here -->
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
            updateDetectionDisplay(data);
        });
        
        socket.on('barrier_status', function(data) {
            updateBarrierStatus(data);
        });
        
        socket.on('system_stats', function(data) {
            updateSystemStats(data);
        });
        
        // Control functions
        function controlBarrier(action) {
            fetch('/api/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: 'barrier', value: action})
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
        
        // UI update functions
        function updateDetectionDisplay(data) {
            const logDiv = document.getElementById('detection-log');
            const detectionItem = document.createElement('div');
            detectionItem.className = 'detection-item';
            
            if (data.bus_detected) {
                detectionItem.className += ' bus-detected';
            }
            
            detectionItem.innerHTML = `
                <strong>${new Date(data.timestamp * 1000).toLocaleTimeString()}</strong> - 
                Bus: ${data.bus_detected ? '✅' : '❌'} 
                (${data.confidence.toFixed(2)}) - 
                Action: ${data.action || 'None'}
            `;
            
            logDiv.insertBefore(detectionItem, logDiv.firstChild);
            
            // Keep only last 50 entries
            while (logDiv.children.length > 50) {
                logDiv.removeChild(logDiv.lastChild);
            }
            
            // Update last detection
            if (data.bus_detected) {
                document.getElementById('last-detection').textContent = 
                    new Date(data.timestamp * 1000).toLocaleTimeString();
            }
        }
        
        function updateBarrierStatus(data) {
            const statusElement = document.getElementById('barrier-status');
            const panelElement = document.getElementById('status-panel');
            
            statusElement.textContent = data.status;
            
            if (data.status === 'DOWN') {
                panelElement.className = 'status-panel status-down';
            } else {
                panelElement.className = 'status-panel status-up';
            }
        }
        
        function updateSystemStats(data) {
            document.getElementById('total-frames').textContent = data.total_frames;
            document.getElementById('bus-detections').textContent = data.bus_detections;
            document.getElementById('detection-count').textContent = data.detection_count;
        }
    </script>
</body>
</html>
            """, esp32_cam_ip=self.esp32_cam_ip)
        
        @self.app.route('/api/control', methods=['POST'])
        def control_train():
            """Manual control endpoint"""
            try:
                data = request.json
                command = data.get('command')
                value = data.get('value')
                
                # Send command to ESP32 train
                result = self.send_command_to_train(command, value)
                
                # Broadcast to WebSocket clients
                self.socketio.emit('barrier_status', {
                    'status': value.upper(),
                    'timestamp': time.time(),
                    'manual': True
                })
                
                return jsonify({
                    'status': 'success',
                    'command': command,
                    'value': value,
                    'result': result
                })
                
            except Exception as e:
                return jsonify({'status': 'error', 'message': str(e)}), 500
        
        @self.app.route('/api/detection/start', methods=['POST'])
        def start_detection():
            """Start detection loop"""
            if not self.detection_running:
                self.start_detection_loop()
                return jsonify({'status': 'success', 'message': 'Detection started - OpenCV window will appear'})
            else:
                return jsonify({'status': 'info', 'message': 'Detection already running'})
        
        @self.app.route('/api/detection/stop', methods=['POST'])
        def stop_detection():
            """Stop detection loop"""
            self.stop_detection_loop()
            return jsonify({'status': 'success', 'message': 'Detection stopped - OpenCV window closed'})
        
        @self.app.route('/api/status')
        def get_status():
            """Get current system status"""
            return jsonify({
                'barrier_state': self.barrier_state,
                'detection_running': self.detection_running,
                'total_frames': self.total_frames_processed,
                'detection_count': self.detection_count,
                'last_detection': self.last_detection_time,
                'current_detections': self.current_detections
            })
    
    def setup_socketio_handlers(self):
        """Setup WebSocket event handlers"""
        
        @self.socketio.on('connect')
        def handle_connect():
            print('Client connected to WebSocket')
            emit('system_stats', {
                'total_frames': self.total_frames_processed,
                'bus_detections': self.detection_count,
                'detection_count': self.detection_count
            })
    
    def test_connections(self):
        """Test connections to both ESP32 devices"""
        print("🔧 Testing device connections...")
        
        # Test ESP32-CAM
        try:
            response = requests.get(self.capture_url, timeout=5)
            if response.status_code == 200:
                print("✅ ESP32-CAM connection successful!")
                cam_ok = True
            else:
                print("❌ ESP32-CAM connection failed!")
                cam_ok = False
        except Exception as e:
            print(f"❌ ESP32-CAM error: {e}")
            cam_ok = False
        
        # Test ESP32-Train
        try:
            response = requests.get(f"http://{self.esp32_train_ip}/status", timeout=5)
            train_ok = response.status_code == 200
            if train_ok:
                print("✅ ESP32-Train connection successful!")
            else:
                print("❌ ESP32-Train connection failed!")
        except Exception as e:
            print(f"❌ ESP32-Train error: {e}")
            train_ok = False
        
        return cam_ok, train_ok
    
    def capture_frame_from_camera(self):
        """Capture frame from ESP32-CAM"""
        try:
            response = requests.get(self.capture_url, timeout=3)
            if response.status_code == 200:
                nparr = np.frombuffer(response.content, np.uint8)
                frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                return frame
            return None
        except Exception as e:
            print(f"Frame capture error: {e}")
            return None
    
    def detect_objects(self, frame):
        """Run YOLO detection on frame and return annotated frame"""
        try:
            results = self.model(frame, conf=self.conf_threshold, verbose=False)
            
            detections = {"bus": False, "car": False}
            max_confidence = 0.0
            
            # Create annotated frame with bounding boxes
            annotated_frame = frame.copy()
            
            for result in results:
                boxes = result.boxes
                if boxes is not None:
                    for box in boxes:
                        cls_id = int(box.cls[0])
                        confidence = float(box.conf[0])
                        class_name = self.model.names[cls_id].lower()
                        
                        if class_name in detections:
                            detections[class_name] = True
                            if confidence > max_confidence:
                                max_confidence = confidence
                            
                            # Draw bounding box
                            xyxy = box.xyxy[0].cpu().numpy().astype(int)
                            x1, y1, x2, y2 = xyxy
                            
                            # Choose color based on class
                            if class_name == "bus":
                                color = (0, 0, 255)  # Red for bus (BGR format)
                                thickness = 4  # Thicker for bus (more important)
                            elif class_name == "car":
                                color = (0, 255, 0)  # Green for car (BGR format)
                                thickness = 2
                            
                            # Draw rectangle
                            cv2.rectangle(annotated_frame, (x1, y1), (x2, y2), color, thickness)
                            
                            # Draw label background
                            label = f"{class_name.upper()}: {confidence:.2f}"
                            label_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.7, 2)[0]
                            cv2.rectangle(annotated_frame, 
                                        (x1, y1 - label_size[1] - 10), 
                                        (x1 + label_size[0] + 10, y1), 
                                        color, -1)
                            
                            # Draw label text
                            cv2.putText(annotated_frame, label, 
                                      (x1 + 5, y1 - 5), 
                                      cv2.FONT_HERSHEY_SIMPLEX, 0.7, 
                                      (255, 255, 255), 2)
            
            # Add status overlay on frame
            overlay_height = 100
            cv2.rectangle(annotated_frame, (0, 0), (annotated_frame.shape[1], overlay_height), (0, 0, 0), -1)
            cv2.rectangle(annotated_frame, (0, 0), (annotated_frame.shape[1], overlay_height), (255, 255, 255), 2)
            
            # Barrier status
            barrier_color = (0, 0, 255) if self.barrier_state == "DOWN" else (0, 255, 0)
            barrier_text = f"BARRIER: {self.barrier_state}"
            cv2.putText(annotated_frame, barrier_text, (10, 30), 
                       cv2.FONT_HERSHEY_SIMPLEX, 1, barrier_color, 2)
            
            # Detection status
            bus_status = "BUS: DETECTED" if detections['bus'] else "BUS: CLEAR"
            car_status = "CAR: DETECTED" if detections['car'] else "CAR: CLEAR"
            
            bus_color = (0, 0, 255) if detections['bus'] else (128, 128, 128)
            car_color = (0, 255, 0) if detections['car'] else (128, 128, 128)
            
            cv2.putText(annotated_frame, bus_status, (10, 60), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, bus_color, 2)
            cv2.putText(annotated_frame, car_status, (250, 60), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, car_color, 2)
            
            # Frame counter and confidence
            frame_info = f"Frame: {self.total_frames_processed} | Max Conf: {max_confidence:.2f}"
            cv2.putText(annotated_frame, frame_info, (10, 85), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            
            return detections, max_confidence, annotated_frame
            
        except Exception as e:
            print(f"Detection error: {e}")
            return {"bus": False, "car": False}, 0.0, frame
    
    def send_command_to_train(self, command, value):
        """Send HTTP command to ESP32 train with improved error handling"""
        try:
            payload = {
                "command": command,
                "value": value,
                "timestamp": time.time()
            }
            
            # Increased timeout and retry logic
            max_retries = 2
            timeout_seconds = 10  # Increased from 5 to 10 seconds
            
            for attempt in range(max_retries):
                try:
                    response = requests.post(
                        self.train_control_url,
                        json=payload,
                        timeout=timeout_seconds,
                        headers={'Content-Type': 'application/json'}
                    )
                    
                    if response.status_code == 200:
                        print(f"✅ Command sent: {command}={value}")
                        self.last_command_sent = f"{command}={value}"
                        if command == "barrier":
                            self.barrier_state = value.upper()
                        return True
                    else:
                        print(f"⚠️  Command response {response.status_code}: {command}={value}")
                        if attempt < max_retries - 1:
                            print(f"🔄 Retrying... (attempt {attempt + 2}/{max_retries})")
                            time.sleep(1)
                        else:
                            return False
                            
                except requests.exceptions.Timeout:
                    print(f"⏱️  Timeout on attempt {attempt + 1}/{max_retries} for {command}={value}")
                    if attempt < max_retries - 1:
                        print(f"🔄 Retrying with longer timeout...")
                        time.sleep(2)  # Wait longer before retry
                    else:
                        print(f"❌ Final timeout - ESP32 may be busy moving servo")
                        # Still consider it successful since servo might be moving
                        self.last_command_sent = f"{command}={value} (timeout)"
                        if command == "barrier":
                            self.barrier_state = value.upper()
                        return True  # Return True because command was likely received
                        
                except requests.exceptions.ConnectionError as e:
                    print(f"🔌 Connection error attempt {attempt + 1}: {e}")
                    if attempt < max_retries - 1:
                        time.sleep(2)
                    else:
                        return False
                        
        except Exception as e:
            print(f"❌ Unexpected error sending command: {e}")
            return False
    
    def process_detection_logic(self, detections, confidence):
        """Smart detection logic with consecutive detection filtering"""
        current_time = time.time()
        
        # Add to history
        self.detection_history.append({
            "timestamp": current_time,
            "bus": detections["bus"],
            "confidence": confidence
        })
        
        # Keep only recent history (last 10 seconds)
        cutoff_time = current_time - 10
        self.detection_history = [
            h for h in self.detection_history 
            if h["timestamp"] > cutoff_time
        ]
        
        # Check for consecutive detections
        if len(self.detection_history) >= self.consecutive_detections_needed:
            recent = self.detection_history[-self.consecutive_detections_needed:]
            consecutive_bus = all(h["bus"] for h in recent)
            avg_confidence = np.mean([h["confidence"] for h in recent if h["bus"]] or [0])
            
            action_taken = None
            
            # Decision logic
            if consecutive_bus and self.barrier_state == "UP" and avg_confidence > 0.7:
                # Lower barrier
                print(f"🚨 Lowering barrier - {self.consecutive_detections_needed} consecutive bus detections (avg conf: {avg_confidence:.2f})")
                if self.send_command_to_train("barrier", "down"):
                    action_taken = "BARRIER_LOWERED"
                    self.detection_count += 1
                    
            elif not detections["bus"] and self.barrier_state == "DOWN":
                # Check if no bus detected for last few frames
                recent_no_bus = all(not h["bus"] for h in self.detection_history[-2:])
                if recent_no_bus:
                    print(f"✅ Raising barrier - no bus detected")
                    if self.send_command_to_train("barrier", "up"):
                        action_taken = "BARRIER_RAISED"
        
            # Broadcast detection via WebSocket (with error handling)
            try:
                self.socketio.emit('detection_update', {
                    'bus_detected': detections["bus"],
                    'car_detected': detections["car"],
                    'confidence': confidence,
                    'timestamp': current_time,
                    'action': action_taken,
                    'barrier_state': self.barrier_state
                })
                
                # Update stats
                self.socketio.emit('system_stats', {
                    'total_frames': self.total_frames_processed,
                    'bus_detections': self.detection_count,
                    'detection_count': self.detection_count
                })
            except Exception as e:
                # Don't let WebSocket errors break the main loop
                pass
        
        self.current_detections = detections
        if detections["bus"]:
            self.last_detection_time = current_time
    
    def detection_loop(self):
        """Main detection loop running in separate thread"""
        print("🚀 Starting detection loop...")
        
        # Setup OpenCV window
        window_name = "Smart Train - Live Detection"
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
                    
                    # Display info in console
                    if detections["bus"]:
                        print(f"🚌 Bus detected! Confidence: {confidence:.2f}")
                    elif detections["car"]:
                        print(f"🚗 Car detected! Confidence: {confidence:.2f}")
                    
                    # Check for quit key
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q'):
                        print("🛑 'q' pressed - stopping detection")
                        self.detection_running = False
                        break
                    elif key == ord('s'):
                        # Save screenshot
                        timestamp = int(time.time())
                        filename = f"detection_screenshot_{timestamp}.jpg"
                        cv2.imwrite(filename, annotated_frame)
                        print(f"📸 Screenshot saved: {filename}")
                
                time.sleep(0.1)  # 10 FPS
                
            except Exception as e:
                print(f"Detection loop error: {e}")
                time.sleep(1)
        
        # Cleanup
        cv2.destroyAllWindows()
        print("🛑 Detection loop stopped")
    
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
        print(f"\n🌐 Starting Smart Train Server on http://{host}:{port}")
        print(f"📊 Dashboard: http://{host}:{port}")
        print(f"🔌 WebSocket: ws://{host}:{port}")
        
        self.socketio.run(self.app, host=host, port=port, debug=debug)

def main():
    # Configuration
    ESP32_CAM_IP = "192.168.1.31"        # Your ESP32-CAM IP
    ESP32_TRAIN_IP = "192.168.1.39"      # Your ESP32-Train IP  
    MODEL_PATH = "./runs/detect/train/weights/best.pt"  # Your YOLO model
    
    # Verify model exists
    if not os.path.exists(MODEL_PATH):
        print(f"❌ Model file not found: {MODEL_PATH}")
        print("Please check the path to your YOLO model file")
        return
    
    # Create server
    server = SmartTrainServer(ESP32_CAM_IP, ESP32_TRAIN_IP, MODEL_PATH)
    
    # Test connections
    cam_ok, train_ok = server.test_connections()
    if not cam_ok:
        print("⚠️  ESP32-CAM not connected - some features may not work")
    if not train_ok:
        print("⚠️  ESP32-Train not connected - manual control only")
    
    # Start detection automatically if camera is available
    if cam_ok:
        server.start_detection_loop()
    
    # Run server
    try:
        server.run_server(port=5000, debug=False)
    except KeyboardInterrupt:
        print("\n🛑 Shutting down server...")
        server.stop_detection_loop()

if __name__ == "__main__":
    main()