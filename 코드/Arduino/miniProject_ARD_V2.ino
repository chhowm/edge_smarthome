/*
 WiFiEsp test: ClientTest
http://www.kccistc.net/
작성일 : 2019.12.17 
작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI
#define AP_SSID "embB"
#define AP_PASS "embB1234"
#define SERVER_NAME "10.10.141.82"
#define SERVER_PORT 5000  
#define LOGID "HCY_ARD"
#define PASSWD "PASSWD"

#define WIFITX 7  //7:TX -->ESP8266 RX
#define WIFIRX 6 //6:RX-->ESP8266 TX
#define RELAY1 3 //릴레이 1
#define RELAY2 4 //릴레이 2
#define LED_PIN 12

#define CMD_SIZE 50
#define ARR_CNT 5           

#include "WiFiEsp.h"
#include "SoftwareSerial.h"
#include <TimerOne.h>

char sendBuf[CMD_SIZE];
bool timerIsrFlag = false;

unsigned int secCount;
int sensorTime;

SoftwareSerial wifiSerial(WIFIRX, WIFITX); 
WiFiEspClient client;

void setup() {
  // put your setup code here, to run once:
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(LED_PIN, OUTPUT);  
    Serial.begin(115200); //DEBUG
    wifi_Setup();
    Timer1.initialize(1000000);
    Timer1.attachInterrupt(timerIsr); // timerIsr to run every 1 seconds
}

void loop() {
  // put your main code here, to run repeatedly:
  if(client.available()) {
    socketEvent();
  }
  if (timerIsrFlag)
  {
    timerIsrFlag = false; 
    if(!(secCount%5))
    {
      if (!client.connected()) { 
        server_Connect();
      }
    }
  }
}
void socketEvent()
{
  int i=0;
  char * pToken;
  char * pArray[ARR_CNT]={0};
  char recvBuf[CMD_SIZE]={0}; 
  int len;

  len =client.readBytesUntil('\n',recvBuf,CMD_SIZE); 
//  recvBuf[len] = '\0';
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf,"[@]");
  while(pToken != NULL)
  {
    pArray[i] =  pToken;
    if(++i >= ARR_CNT)
      break;
    pToken = strtok(NULL,"[@]");
  }
  if(!strncmp(pArray[1]," New",4))  // New Connected
  {
    Serial.write('\n');
    return ;
  }
  else if(!strncmp(pArray[1]," Alr",4)) //Already logged
  {
    Serial.write('\n');
    client.stop();
    server_Connect();
    return ;
  }   
  if (!strcmp(pArray[1], "RELAY1")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(RELAY1, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(RELAY1, LOW);
    }
  }
  if (!strcmp(pArray[1], "RELAY2")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(RELAY2, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(RELAY2, LOW);
    }
  }
  if (!strcmp(pArray[1], "LED")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_PIN, LOW);
    }
  }
  client.write(sendBuf,strlen(sendBuf));
  client.flush();

#ifdef DEBUG
  Serial.println(", send : ");
  Serial.print(sendBuf);
#endif
}
void timerIsr()
{
//  digitalWrite(LED_BUILTIN_PIN,!digitalRead(LED_BUILTIN_PIN));
  timerIsrFlag = true;
  secCount++;
}
void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}
void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI    
      Serial.println("WiFi shield not present");
#endif 
    }
    else
      break;   
  }while(1);

#ifdef DEBUG_WIFI    
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif     
    while(WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {   
#ifdef DEBUG_WIFI  
      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(AP_SSID);   
#endif   
    }
#ifdef DEBUG_WIFI      
  Serial.println("You're connected to the network");    
  printWifiStatus();
#endif 
}
int server_Connect()
{
#ifdef DEBUG_WIFI     
  Serial.println("Starting connection to server...");
#endif  

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI     
    Serial.println("Connected to server");
#endif  
    client.print("["LOGID":"PASSWD"]"); 
  }
  else
  {
#ifdef DEBUG_WIFI      
     Serial.println("server connection failure");
#endif    
  } 
}
void printWifiStatus()
{
  // print the SSID of the network you're attached to
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
