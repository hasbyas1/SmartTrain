#include <ESP32Servo.h>

Servo myServo;

const int servoPin = 14;     // Ubah sesuai dengan pin yang kamu pakai
unsigned long previousMillis = 0;
const long interval = 5000;  // 5 detik
bool toggle = false;

void setup() {
  Serial.begin(115200);
  myServo.attach(servoPin);
  myServo.write(0);  // Awal posisi 0 derajat
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (toggle) {
      myServo.write(0);     // posisi 0 derajat
      Serial.println("Servo ke 0 derajat");
    } else {
      myServo.write(90);    // posisi 90 derajat
      Serial.println("Servo ke 90 derajat");
    }

    toggle = !toggle; // ubah arah setiap 5 detik
  }
}
