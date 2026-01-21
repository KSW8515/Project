#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <IRremote.hpp>
#include <SPI.h>
#include <MFRC522.h>
#include <stdlib.h>

#define AP_SSID "robotA"
#define AP_PASS "robotA1234"
#define SERVER_NAME "10.10.141.61"
#define SERVER_PORT 5000
#define LOGID "DOOR1"
#define PASSWD "1111"
#define PERSON_DETECT_DISTANCE 6

#define STMID "DOOR2"
#define SQLID "DOOR3"

#define IR_PIN A1
#define IR_FEED 4

#define BTN 3
#define LED_R 4
#define LED_G 5

#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define RST_PIN 9
#define SS_PIN 10

#define TRIG 2
#define ECHO 8

#define ARR_CNT 5
#define CMD_SIZE 50

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
byte nuidPICC[4];

char sendBuf[CMD_SIZE];
enum READ_MODE
{
  attendance = 0,
  leave = 1,
  add = 2,
};

bool read_lock = false;
bool btn_lock = false;

READ_MODE now_mode = attendance;

SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;

void setup() 
{
  Serial.begin(9600);  
  wifi_Setup();

  IrReceiver.begin(IR_PIN, IR_FEED);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(BTN, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
}

void loop() 
{
  if (client.available()) 
  {
    socketEvent();
  }

  if (IrReceiver.decode())
  {
    int xxx =IrReceiver.decodedIRData.decodedRawData;

    switch(IrReceiver.decodedIRData.decodedRawData)
    {
      case 0xF30CFF00:
        now_mode = attendance;
        read_lock = false;
        
        sprintf(sendBuf, "[%s]%s@%s\n", STMID, "DOOR", "ON");
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        break;
      case 0xE718FF00:
        now_mode = add;
        read_lock = false;
        
        sprintf(sendBuf, "[%s]%s@%s\n", STMID, "ADD", "USER");
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        break;
      case 0xA15EFF00:
        now_mode = leave;
        read_lock = false;
        
        sprintf(sendBuf, "[%s]%s@%s\n", STMID, "LEV", "ON");
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        break;
    }
    IrReceiver.resume();
  }

  if (digitalRead(BTN) && !btn_lock)
  {
    long current_dist = get_distance();

    if (current_dist >= PERSON_DETECT_DISTANCE)
    {
       btn_lock = true;
       request_leave_check(); // 등원/하원 완료 요청 전송
    }
    else 
    {
      for(int i = 0; i < 5; ++i)
      {                              
        digitalWrite(LED_R, HIGH);
        delay(100);
        digitalWrite(LED_R, LOW);
        delay(100);
      }
    }
  }

  bool isNewCardPresent = rfid.PICC_IsNewCardPresent();
  delay(100);
  bool isReadCardSerial = rfid.PICC_ReadCardSerial();
  bool detected = isNewCardPresent && isReadCardSerial;

  if (!detected)
  {
    memset(nuidPICC, 0, sizeof(nuidPICC));
    return;
  }
  else
  {
    if (rfid.uid.uidByte[0] == nuidPICC[0] && rfid.uid.uidByte[1] == nuidPICC[1] &&
        rfid.uid.uidByte[2] == nuidPICC[2] && rfid.uid.uidByte[3] == nuidPICC[3])
    {
      return;
    }
  }

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  // 카드나 태그인지 확인
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) 
    return;
  
  Serial.println("Read Card");

  if (read_lock)
    return;

  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) 
  {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  if (now_mode == attendance)
  {
    request_open_door();
    read_lock = true;
  }
  else if (now_mode == leave)
  {
    request_student_leave();
    read_lock = true;
  }
  else if (now_mode == add)
  {
    request_student_add();
    read_lock = true;
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}
///
void socketEvent() 
{
  int i = 0;
  char *pToken;
  char *pArray[ARR_CNT] = { 0 };
  char recvBuf[CMD_SIZE] = { 0 };
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();

  Serial.print("recv : ");
  Serial.println(recvBuf);

  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) 
  {
    pArray[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  
  if (!strcmp(pArray[1], "DOOR"))
  {
    // 문 열기 종료
    if (!strcmp(pArray[2], "OPEN"))
    {
      read_lock = false;
      Serial.println("UnLock");
    }
    else if (!strcmp(pArray[2], "FAIL"))
    {
      read_lock = false;
      Serial.println("UnLock");
    }
  }
  else if (!strcmp(pArray[1], "ADD")) 
  {
    // 사용자 추가 모드 활성화
    if (!strcmp(pArray[2], "USER")) 
    {
      now_mode = add;
      read_lock = false;
      Serial.println("UnLock");
      sprintf(sendBuf, "[%s]%s@%s\n", STMID, "ADD", "USER");
      client.write(sendBuf, strlen(sendBuf));
      client.flush();

      Serial.print("send : ");
      Serial.print(sendBuf);
    }
    // 사용자 추가 모드 종료
    else if (!strcmp(pArray[2], "FINISH"))
    {
      now_mode = attendance;
      read_lock = false;
      Serial.println("UnLock");
    }
  }
  // 출석모드 전환
  else if (!strcmp(pArray[1], "ATD")) 
  {
    if (!strcmp(pArray[2], "ON"))
    {
      now_mode = attendance;
      read_lock = false;
      Serial.println("UnLock");
      sprintf(sendBuf, "[%s]%s@%s\n", STMID, "DOOR", "ON");
      client.write(sendBuf, strlen(sendBuf));
      client.flush();

      Serial.print("send : ");
      Serial.print(sendBuf);
    }
  }
  else if (!strcmp(pArray[1], "LEV")) 
  {
    // 하원모드 전환
    if (!strcmp(pArray[2], "ON"))
    {
      now_mode = leave;
      read_lock = false;
      Serial.println("UnLock");
      sprintf(sendBuf, "[%s]%s@%s\n", STMID, "LEV", "ON");
      client.write(sendBuf, strlen(sendBuf));
      client.flush();

      Serial.print("send : ");
      Serial.print(sendBuf);
    }
    else if (!strcmp(pArray[2], "FINISH"))
    {
      read_lock = false;
      Serial.println("UnLock");
    }
    // 하원체크 성공
    else if (!strcmp(pArray[2], "SUCCESS"))
    {
      for(int i = 0; i < 5; ++i)
      {
        digitalWrite(LED_G, HIGH);
        delay(100);
        digitalWrite(LED_G, LOW);
        delay(100);
      }
      btn_lock = false;
    }
    // 하원체크 실패
    else if (!strcmp(pArray[2], "FAIL"))
    {
      for(int i = 0; i < 5; ++i)
      {                              
        digitalWrite(LED_R, HIGH);
        delay(100);
        digitalWrite(LED_R, LOW);
        delay(100);
      }
      btn_lock = false;
    }
  }
  else
    return;
}

void wifi_Setup() 
{
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}

void wifi_Init() 
{
  do 
  {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) 
    {
      Serial.println("WiFi shield not present");
    } 
    else
      break;
  } while (1);


  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);

  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) 
  {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
  }

  Serial.println("You're connected to the network");
  printWifiStatus();
}

int server_Connect() 
{
  Serial.println("Starting connection to server...");

  if (client.connect(SERVER_NAME, SERVER_PORT)) 
  {
    Serial.println("Connect to server");

    client.print("[" LOGID ":" PASSWD "]");
  } 
  else 
  {
    Serial.println("server connection failure");
  }
}

void printWifiStatus() 
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void request_open_door()
{
  char uidStr[9];

  snprintf(uidStr, sizeof(uidStr), "%02X%02X%02X%02X",
           rfid.uid.uidByte[0], rfid.uid.uidByte[1],
           rfid.uid.uidByte[2], rfid.uid.uidByte[3]);
  Serial.println("Request Open Door");
  sprintf(sendBuf, "[%s]DOOR@OPEN@%s\n", SQLID, uidStr);
  client.write(sendBuf);
  client.flush();
}

void request_student_leave()
{
  char uidStr[9];

  snprintf(uidStr, sizeof(uidStr), "%02X%02X%02X%02X",
           rfid.uid.uidByte[0], rfid.uid.uidByte[1],
           rfid.uid.uidByte[2], rfid.uid.uidByte[3]);
  Serial.println("Request Student Leave");
  sprintf(sendBuf, "[%s]LEAVE@%s\n", SQLID, uidStr);
  client.write(sendBuf);
  client.flush();
}

void request_student_add()
{
  char uidStr[9];

  snprintf(uidStr, sizeof(uidStr), "%02X%02X%02X%02X",
           rfid.uid.uidByte[0], rfid.uid.uidByte[1],
           rfid.uid.uidByte[2], rfid.uid.uidByte[3]);
  
  Serial.println("Request Student Add");
  sprintf(sendBuf, "[%s]ADD@%s\n", SQLID, uidStr);
  client.write(sendBuf);
  client.flush();
}

void request_leave_check()
{
  Serial.println("Request Leave Check");
  sprintf(sendBuf, "[%s]LEAVE@CHECK\n", SQLID);
  client.write(sendBuf);
  client.flush();
}

long get_distance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  long distance = duration * 17 / 1000;
  
  return distance;
}