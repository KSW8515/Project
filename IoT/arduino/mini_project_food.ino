/*
  WiFiEsp + RFID Fixed Version
  기능: RFID 태그 인식 -> WiFi로 서버 전송
  수정: 서버 다운 방지를 위한 전송 속도 제한 (Rate Limiting, 0.2초) 적용
*/

#define DEBUG // 시리얼 모니터 출력용

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MsTimer2.h>

// --- 1. 와이파이 설정 ---
#define AP_SSID "robotA"
#define AP_PASS "robotA1234"
#define SERVER_NAME "10.10.141.61" // 라즈베리파이 IP
#define SERVER_PORT 5000           // 라즈베리파이 포트
#define LOGID "LDY_ARD"            // 기기 ID
#define PASSWD "PASSWD"

// --- 2. 핀 설정 ---
#define WIFIRX 6  // 와이파이 TX -> 아두이노 D6
#define WIFITX 7  // 와이파이 RX -> 아두이노 D7

// RFID 핀 (SPI)
#define SS_PIN 10
#define RST_PIN 9
// MOSI: 11, MISO: 12, SCK: 13 (자동 할당됨)

// --- 3. 객체 및 변수 ---
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
MFRC522 rfid(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;
char sendId[10] = "LDY_ARD";
char sendBuf[50];
bool timerIsrFlag = false;

// [중요] 전송 속도 제어를 위한 변수 추가
unsigned long lastRfidSendTime = 0;
const long rfidInterval = 200; // 0.2초(200ms) 간격으로만 전송 허용

void setup() {
#ifdef DEBUG
  Serial.begin(115200); // 시리얼 모니터 속도
  Serial.println(F("System Start..."));
#endif

  // 와이파이 초기화
  wifi_Setup();

  // RFID 초기화
  SPI.begin();
  rfid.PCD_Init();
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
#ifdef DEBUG
  Serial.println(F("RFID Reader Ready (Rate Limited)."));
#endif

  // 타이머 설정 (서버 연결 유지를 위한 주기적 체크용)
  MsTimer2::set(1000, timerIsr);
  MsTimer2::start();
}

void loop() {
  // 1. 서버 데이터 수신 확인 (연결 유지용)
  if (client.available()) {
    char c = client.read();
    // 필요시 Serial.write(c);
  }

  // 2. 타이머 주기적 실행 (1초마다)
  if (timerIsrFlag) {
    timerIsrFlag = false;

    // 서버 연결 끊기면 재접속 시도
    if (!client.connected()) {
#ifdef DEBUG
      Serial.println("Server Disconnected. Reconnecting...");
#endif
      server_Connect();
    }
  }

  // 3. RFID 태그 인식 및 전송 (속도 제한 적용)
  // 현재 시간이 마지막 전송 시간보다 200ms 지났을 때만 실행
  if (millis() - lastRfidSendTime >= rfidInterval) {
    rfid_Check();
  }
}

// --- RFID 처리 함수 ---
void rfid_Check() {
  // 1. 새 카드가 없거나, 시리얼을 읽지 못하면 리턴
  if ( !rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial() ) {
    return;
  }

  // 2. MIFARE 타입만 허용
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    return;
  }

  // 3. 16진수 문자열로 변환
  char hexId[10] = {0};
  sprintf(hexId, "%02X%02X%02X%02X", rfid.uid.uidByte[0], rfid.uid.uidByte[1], rfid.uid.uidByte[2], rfid.uid.uidByte[3]);

  // 4. 전송 패킷 생성
  // 주의: 만약 [LDY_STM] 헤더 때문에 여전히 충돌이 난다면 [ALLMSG]로 변경하세요.
  sprintf(sendBuf, "[LDY_LIN]RFID@%s\n", hexId);

  // 5. 서버로 전송
  client.write(sendBuf, strlen(sendBuf));
  client.flush();

  // [중요] 마지막 전송 시간 갱신 (폭주 방지 핵심)
  lastRfidSendTime = millis();

  // 시리얼 모니터 확인용
#ifdef DEBUG
  Serial.print("RFID Sent: ");
  Serial.print(sendBuf);
#endif

  // 6. 리더기 상태 초기화
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// --- 타이머 인터럽트 ---
void timerIsr() {
  timerIsrFlag = true;
}

// --- 와이파이 초기화 ---
void wifi_Setup() {
  wifiSerial.begin(38400); // 사용자 환경: 38400

  WiFi.init(&wifiSerial);
  if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG
    Serial.println("WiFi Shield not found");
#endif
    while (true);
  }

#ifdef DEBUG
  Serial.print("Connecting to AP...");
#endif

  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG
    Serial.print(".");
#endif
    delay(500);
  }

#ifdef DEBUG
  Serial.println("\nWiFi Connected.");
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: "); Serial.println(ip);
#endif

  server_Connect();
}

void server_Connect() {
  if (client.connect(SERVER_NAME, SERVER_PORT)) {
    client.print("[" LOGID ":" PASSWD "]"); // 로그인 패킷
#ifdef DEBUG
    Serial.println("Server Connected!");
#endif
  } else {
#ifdef DEBUG
    Serial.println("Server Connect Failed");
#endif
  }
}