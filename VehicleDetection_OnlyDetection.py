import cv2
import numpy as np
import requests
from ultralytics import YOLO
import time
import os

class VehicleDetector:
    def __init__(self, esp32_cam_ip, model_path):
        """
        Simple Vehicle Detector
        Hanya untuk deteksi bus dan car, tidak ada kontrol otomatis
        """
        self.esp32_cam_ip = esp32_cam_ip
        self.capture_url = f"http://{esp32_cam_ip}/capture"
        
        # Load YOLO model
        print(f"Loading YOLO model from: {model_path}")
        self.model = YOLO(model_path)
        self.conf_threshold = 0.6
        
        # Statistics
        self.total_frames = 0
        self.bus_detected_count = 0
        self.car_detected_count = 0
        
        print(f"✅ Vehicle Detector Initialized")
        print(f"📷 ESP32-CAM: {esp32_cam_ip}")
        print(f"🤖 Model loaded: {model_path}")
    
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
    
    def capture_frame(self):
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
    
    def detect_vehicles(self, frame):
        """
        Detect bus and car using YOLO
        Returns: detections dict, max_confidence, annotated_frame
        """
        try:
            # Run YOLO detection
            results = self.model(frame, conf=self.conf_threshold, verbose=False)
            
            detections = {"bus": False, "car": False}
            max_confidence = 0.0
            detected_objects = []
            
            # Create annotated frame
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
                        detected_objects.append({
                            'class': class_name,
                            'confidence': conf
                        })
                        
                        # Draw bounding box
                        x1, y1, x2, y2 = map(int, box.xyxy[0])
                        
                        # Color: Green for bus, Blue for car
                        color = (0, 255, 0) if class_name == 'bus' else (255, 0, 0)
                        cv2.rectangle(annotated_frame, (x1, y1), (x2, y2), color, 2)
                        
                        # Draw label with background
                        label = f"{class_name.upper()}: {conf:.2f}"
                        label_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)
                        cv2.rectangle(annotated_frame, (x1, y1 - label_size[1] - 10), 
                                    (x1 + label_size[0], y1), color, -1)
                        cv2.putText(annotated_frame, label, (x1, y1 - 5),
                                  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            
            return detections, max_confidence, annotated_frame, detected_objects
            
        except Exception as e:
            print(f"Detection error: {e}")
            return {"bus": False, "car": False}, 0.0, frame, []
    
    def add_info_overlay(self, frame, detections, detected_objects):
        """Add information overlay to frame"""
        height, width = frame.shape[:2]
        
        # Create semi-transparent overlay panel
        overlay = frame.copy()
        cv2.rectangle(overlay, (10, 10), (width - 10, 120), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.6, frame, 0.4, 0, frame)
        
        # Add text information
        y_offset = 35
        cv2.putText(frame, f"Frames: {self.total_frames}", (20, y_offset),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        
        y_offset += 30
        cv2.putText(frame, f"Bus Count: {self.bus_detected_count}", (20, y_offset),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        
        y_offset += 30
        cv2.putText(frame, f"Car Count: {self.car_detected_count}", (20, y_offset),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2)
        
        # Show current detection status
        if detected_objects:
            status_text = "DETECTED: " + ", ".join([f"{obj['class'].upper()}({obj['confidence']:.2f})" 
                                                     for obj in detected_objects])
            cv2.putText(frame, status_text, (20, height - 20),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        else:
            cv2.putText(frame, "No vehicle detected", (20, height - 20),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (128, 128, 128), 2)
        
        return frame
    
    def run_detection(self):
        """Main detection loop"""
        print("\n" + "="*60)
        print("🚀 Starting Vehicle Detection")
        print("="*60)
        print("Controls:")
        print("  - Press 'q' to quit")
        print("  - Press 's' to save screenshot")
        print("  - Press 'r' to reset counters")
        print("="*60 + "\n")
        
        # Setup OpenCV window
        window_name = "Vehicle Detection - ESP32-CAM"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 800, 600)
        
        fps_start_time = time.time()
        fps_frame_count = 0
        current_fps = 0
        
        try:
            while True:
                # Capture frame
                frame = self.capture_frame()
                
                if frame is not None:
                    self.total_frames += 1
                    fps_frame_count += 1
                    
                    # Run detection
                    detections, confidence, annotated_frame, detected_objects = self.detect_vehicles(frame)
                    
                    # Update counters
                    if detections["bus"]:
                        self.bus_detected_count += 1
                    if detections["car"]:
                        self.car_detected_count += 1
                    
                    # Add info overlay
                    annotated_frame = self.add_info_overlay(annotated_frame, detections, detected_objects)
                    
                    # Calculate FPS
                    if time.time() - fps_start_time >= 1.0:
                        current_fps = fps_frame_count
                        fps_frame_count = 0
                        fps_start_time = time.time()
                    
                    # Add FPS to frame
                    cv2.putText(annotated_frame, f"FPS: {current_fps}", 
                               (annotated_frame.shape[1] - 120, 35),
                               cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
                    
                    # Display frame
                    cv2.imshow(window_name, annotated_frame)
                    
                    # Print detection info to console
                    if detected_objects:
                        detection_str = ", ".join([f"{obj['class'].upper()}({obj['confidence']:.2f})" 
                                                   for obj in detected_objects])
                        print(f"Frame {self.total_frames}: {detection_str}")
                
                else:
                    print("Failed to capture frame, retrying...")
                    time.sleep(1)
                
                # Handle keyboard input
                key = cv2.waitKey(1) & 0xFF
                
                if key == ord('q'):
                    print("\n'q' pressed - stopping detection")
                    break
                    
                elif key == ord('s'):
                    # Save screenshot
                    timestamp = int(time.time())
                    filename = f"detection_{timestamp}.jpg"
                    cv2.imwrite(filename, annotated_frame)
                    print(f"📸 Screenshot saved: {filename}")
                    
                elif key == ord('r'):
                    # Reset counters
                    self.total_frames = 0
                    self.bus_detected_count = 0
                    self.car_detected_count = 0
                    print("🔄 Counters reset")
                
                time.sleep(0.05)  # ~20 FPS
                
        except KeyboardInterrupt:
            print("\n⏹️ Detection interrupted by user")
        
        finally:
            # Cleanup
            cv2.destroyAllWindows()
            
            # Print final statistics
            print("\n" + "="*60)
            print("📊 Detection Summary")
            print("="*60)
            print(f"Total Frames Processed: {self.total_frames}")
            print(f"Bus Detections: {self.bus_detected_count}")
            print(f"Car Detections: {self.car_detected_count}")
            print("="*60)

def main():
    # ==================== CONFIGURATION ====================
    ESP32_CAM_IP = "192.168.1.31"  # Ganti dengan IP ESP32-CAM kamu
    MODEL_PATH = "./runs/detect/train/weights/best.pt"  # Path ke model YOLO
    # =======================================================
    
    # Verify model exists
    if not os.path.exists(MODEL_PATH):
        print(f"❌ Model file not found: {MODEL_PATH}")
        print("Please provide the correct path to your YOLO model")
        return
    
    # Create detector
    detector = VehicleDetector(ESP32_CAM_IP, MODEL_PATH)
    
    # Test camera connection
    if not detector.test_camera_connection():
        print("\n⚠️ Cannot connect to ESP32-CAM")
        print(f"Please check:")
        print(f"  1. ESP32-CAM is powered on")
        print(f"  2. IP address is correct: {ESP32_CAM_IP}")
        print(f"  3. ESP32-CAM is connected to same network")
        return
    
    # Start detection
    detector.run_detection()

if __name__ == "__main__":
    main()
