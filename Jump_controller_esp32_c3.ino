#include <Wire.h>
#include <MPU6050.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <HIDTypes.h>

MPU6050 mpu;

float jumpThreshold = 1.8;      // higher threshold — only a real jump upward
float landingThreshold = 0.7;   // below this = freefall (you are in the air)
unsigned long jumpCooldown = 400; // longer cooldown to skip landing spike
unsigned long lastJumpTime = 0;

bool inAir = false;             // tracks if you are currently airborne

NimBLEHIDDevice* hid;
NimBLECharacteristic* input;
NimBLEServer* server;
bool connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    connected = true;
    Serial.println("BLE Connected!");
    server->updateConnParams(info.getConnHandle(), 24, 40, 0, 600);
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    connected = false;
    Serial.print("Disconnected, reason: ");
    Serial.println(reason);
    NimBLEDevice::startAdvertising();
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (info.isEncrypted()) {
      Serial.println("Encrypted connection established!");
    } else {
      NimBLEDevice::getServer()->disconnect(info.getConnHandle());
    }
  }
};

void sendSpace() {
  uint8_t press[8]   = {0, 0, 0x2C, 0, 0, 0, 0, 0};
  uint8_t release[8] = {0, 0, 0,    0, 0, 0, 0, 0};
  input->setValue(press, sizeof(press));
  input->notify();
  delay(100);
  input->setValue(release, sizeof(release));
  input->notify();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(8, 9);
  Wire.setClock(100000);
  delay(500);
  mpu.initialize();
  delay(200);
  Serial.println("MPU6050 ready.");

  NimBLEDevice::init("Jump Controller");
  NimBLEDevice::setPower(9);
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  hid = new NimBLEHIDDevice(server);
  input = hid->getInputReport(1);
  hid->setManufacturer("Kesav");
  hid->setHidInfo(0x00, 0x01);

  static const uint8_t reportMap[] = {
    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x85, 0x01,
    0x05, 0x07,
    0x19, 0xe0,
    0x29, 0xe7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x01,
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x65,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x65,
    0x81, 0x00,
    0xC0
  };

  hid->setReportMap((uint8_t*)reportMap, sizeof(reportMap));
  hid->startServices();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid->getHidService()->getUUID());
  advertising->start();

  Serial.println("BLE advertising started!");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;

  Serial.print("AZ: "); Serial.println(az_g, 3);

  unsigned long now = millis();

  // Step 1 — detect jump upward spike
  if (!inAir && az_g > jumpThreshold && (now - lastJumpTime > jumpCooldown)) {
    Serial.println("Jump detected! Sending SPACE...");
    if (connected) sendSpace();
    lastJumpTime = now;
    inAir = true;  // mark as airborne, ignore everything until landing
  }

  // Step 2 — detect freefall (you are in the air, AZ drops below 0.7g)
  if (inAir && az_g < landingThreshold) {
    Serial.println("Airborne...");
  }

  // Step 3 — detect landing (AZ returns to normal after freefall)
  // just reset inAir so next jump can trigger, but do NOT send space
  if (inAir && az_g > 0.9 && az_g < jumpThreshold && (now - lastJumpTime > 150)) {
    Serial.println("Landed.");
    inAir = false;
  }

  delay(20); // faster polling for better jump detection
}
