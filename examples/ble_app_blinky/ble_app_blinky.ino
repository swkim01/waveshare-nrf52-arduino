
#include <bluefruit.h>

/* LedButton 서비스 정의
 * LedButton Service: 0x1523
 * Button    Char:    0x1524
 * LED       Char:    0x1525
 */
const uint8_t LBS_SERVICE_UUID[] =
 {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
  0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00};
const uint8_t LBS_BUTTON_CHAR_UUID[] = 
 {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
  0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00};
const uint8_t LBS_LED_CHAR_UUID[] = 
 {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
  0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00};
BLEService        ledbutton = BLEService(LBS_SERVICE_UUID);
BLECharacteristic button = BLECharacteristic(LBS_BUTTON_CHAR_UUID);
BLECharacteristic led = BLECharacteristic(LBS_LED_CHAR_UUID);

// LED, 버튼 핀 및 상태 정의
const int LED_PIN = 13;
#define LED_OFF LOW
#define LED_ON HIGH

const int BUTTON_PIN = 11;
#define BUTTON_ACTIVE LOW
int lastButtonState = -1;

void setup() 
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // nrf52840 에서 USB 직렬포트를 열어 시작

  Serial.println("Bluefruit52 Blinky Example");
  Serial.println("--------------------------\n");

  pinMode(LED_PIN, OUTPUT); // 온보드 LED 끔
  digitalWrite(LED_PIN, LED_OFF);
  pinMode(BUTTON_PIN, INPUT);

  // CONNECT 시에 활성화되도록 BLE LED 설정
  Bluefruit.autoConnLed(true);

  // 최대 대역으로 주변 기기 연결 설정
  // 주의: config***() 함수는 begin() 이전에 호출되어야 함
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  // 최대 전력 설정. 가능한 값: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Waveshare_nRF52840");
  Bluefruit.setConnectCallback(connect_callback);
  Bluefruit.setDisconnectCallback(disconnect_callback);

  // LedButton 서비스 설정
  Serial.println("Configuring the Blinky Service");
  setupLBS();
  
  // 광고 설정 및 시작
  startAdv();
}

void startAdv(void)
{
  // 광고 패킷
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // LEDButton 서비스의 128비트 uuid
  Bluefruit.Advertising.addService(ledbutton);

  // Secondary Scan Response 패킷 (옵션)
  // 광고 패킷에 'Name' 공간이 없으므로 추가
  Bluefruit.ScanResponse.addName();
  
  /* 광고 시작
   * - 연결이 끊어지면 자동 광고 활성화
   * - 시간 간격:  빠른 모드 = 20 ms, 느린 모드 = 152.5 ms
   * - 빠른 모드의 타임아웃은 30 초
   * - timeout = 0 으로 start(timeout) 실행하면 (연결때까지) 영원히 광고
   * 
   * 권장 광고 시간 간격은 다음 링크 참고
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void setupLBS(void)
{
  // LedButton 서비스 설정
  // 참고: <nRF5 SDK 디렉토리>/components/ble/ble_services/ble_lbs/
  // 지원 특성:
  // 이름        UUID    요구조건    속성
  // ---------- ------  ---------- ----------
  // Button     0x1524     필수    Read | Notify
  // LED        0x1525     필수    Read | Write
  ledbutton.begin();

  // BLEService가 begin() 호출한 후에 내부 특성이 begin() 호출되어야 함
  // begin() 호출한 BLECharacteristic 은 가장 최근에 begin()된 서비스에 추가됨
  // Button 특성 설정
  // 속성
  // Min Len    = 1
  // Max Len    = 1
  //    B0      = UINT8  - 버튼 상태
  button.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  button.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  button.setFixedLen(1);
  button.setCccdWriteCallback(cccd_callback);  // 옵션으로 CCCD 갱신 검사
  button.begin();
  button.notify(0, 1);           // write 대신 notify 사용!

  // Configure the LED 특성 설정
  // 속성 = Read
  // Min Len    = 1
  // Max Len    = 1
  //    B0      = UINT8 - LED ON/OFF
  led.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  led.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  led.setFixedLen(1);
  led.begin();
}

void loop()
{
  if ( Bluefruit.connected() ) {
    uint8_t value = led.read8();
    digitalWrite(LED_PIN, !value);
        
    const uint8_t buttondata = !digitalRead(BUTTON_PIN);
    // 주의: write 대신 notify 사용!
    // 연결되었지만 CCCD가 활성화되지 않았으면
    // notify를 보내지 않아도 특성 값은 갱신
    if ( button.notify(&buttondata, sizeof(buttondata)) ){
      Serial.print("Button State updated to: "); Serial.println(buttondata); 
    }else{
      Serial.println("ERROR: Notify not set in the CCCD or not connected!");
    }
  }

  delay(100);
}

// 센트럴 장치와 연결 시에 호출되는 콜백
void connect_callback(uint16_t conn_handle)
{
  char central_name[32] = { 0 };
  Bluefruit.Gap.getPeerName(conn_handle, central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

// 연결이 끊어졌을 때 호출되는 콜백
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("Disconnected");
}

void cccd_callback(BLECharacteristic& chr, uint16_t cccd_value)
{
    // 요청 패킷 출력
    Serial.print("CCCD Updated: ");
    //Serial.printBuffer(request->data, request->len);
    Serial.print(cccd_value);
    Serial.println("");

    // 이 CCCD 갱신과 연관된 특성 검사
    if (chr.uuid == button.uuid) {
        if (chr.notifyEnabled()) {
            Serial.println("Button 'Notify' enabled");
        } else {
            Serial.println("Button 'Notify' disabled");
        }
    }
}
