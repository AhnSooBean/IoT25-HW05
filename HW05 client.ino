#include <BLEDevice.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// BLE 서버 이름 (서버 코드의 BLEDevice::init("…") 값과 일치해야 함)
#define SERVER_NAME "ESP32_DHT11_Server"

// BLE UUID
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID tempCharUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID humidCharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

// OLED 설정
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// BLE 상태 변수
static boolean doConnect = false;
static boolean connected = false;
static BLEAddress* pServerAddress;
static BLERemoteCharacteristic* tempChar;
static BLERemoteCharacteristic* humidChar;

// 데이터 저장 변수
char tempValue[9] = "0.0";
char humidValue[9] = "0.0";
bool newTemp = false;
bool newHumi = false;

// 서버 탐색 콜백
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == SERVER_NAME) {
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
      Serial.println("서버 발견됨. 연결 시도…");
    }
  }
};

// notify 콜백
static void tempNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (length < sizeof(tempValue)) {
    memcpy(tempValue, pData, length);
    tempValue[length] = '\0';  // null termination
    newTemp = true;
  }
}

static void humidNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (length < sizeof(humidValue)) {
    memcpy(humidValue, pData, length);
    humidValue[length] = '\0';  // null termination
    newHumi = true;
  }
}

// BLE 서버 연결
bool connectToServer(BLEAddress pAddress) {
  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(pAddress)) return false;
  Serial.println("서버에 연결됨.");

  BLERemoteService* pService = pClient->getService(serviceUUID);
  if (pService == nullptr) return false;

  tempChar = pService->getCharacteristic(tempCharUUID);
  humidChar = pService->getCharacteristic(humidCharUUID);
  if (tempChar == nullptr || humidChar == nullptr) return false;

  tempChar->registerForNotify(tempNotifyCallback);
  humidChar->registerForNotify(humidNotifyCallback);

  tempChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)"\x01\x00", 2, true);
  humidChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)"\x01\x00", 2, true);

  return true;
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");

  // OLED 초기화
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED 초기화 실패"));
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("스캔 중...");
  display.display();

  // BLE 스캔 시작
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->start(30);
}

void loop() {
  if (doConnect && !connected) {
    connected = connectToServer(*pServerAddress);
    doConnect = false;

    if (!connected) {
      Serial.println("서버 연결 실패.");
    } else {
      Serial.println("서버 연결 성공.");
    }
  }

  if (connected && newTemp && newHumi) {
    newTemp = false;
    newHumi = false;

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("T: "); display.print(tempValue); display.print("C");
    display.setCursor(0, 32);
    display.print("H: "); display.print(humidValue); display.print("%");
    display.display();

    Serial.print("온도: "); Serial.println(tempValue);
    Serial.print("습도: "); Serial.println(humidValue);
  }

  delay(200);
}