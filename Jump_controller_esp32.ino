#include <Wire.h>
#include <MPU6050.h>
#include <BleKeyboard.h>

MPU6050 mpu;
BleKeyboard bleKeyboard("Jump Controller", "Kesav", 100);

float jumpThreshold = 1.5;
unsigned long jumpCooldown = 500;
unsigned long lastJumpTime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);
  delay(1000);

  mpu.initialize();
  delay(200);

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed! Check wiring.");
    while (1);
  }
  Serial.println("MPU6050 ready.");

  bleKeyboard.begin();
  Serial.println("BLE advertising started.");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;

  Serial.print("AZ: "); Serial.println(az_g, 3);

  if (bleKeyboard.isConnected()) {
    unsigned long now = millis();
    if (az_g > jumpThreshold && (now - lastJumpTime > jumpCooldown)) {
      Serial.println("Jump! Sending SPACE...");
      bleKeyboard.press(' ');
      delay(100);
      bleKeyboard.releaseAll();
      lastJumpTime = now;
    }
  } else {
    Serial.println("Waiting for BLE connection...");
  }

  delay(100);
}
