# IoT 스마트홈 시스템

스마트홈 환경을 구축하기 위한 시스템으로, **STM32**, **Arduino**, **RaspberryPi** 를 사용하여 수집한 **온습도 데이터**를 기반으로 장치를 제어합니다.
* 온습도 데이터를 기반으로 **실시간 장치** 제어
* 앱을 통해 **사용자 제어**가 가능
*  **DB에 저장**된 데이터를 **Grafana**를 통해 시각화

---

### 💡 시스템 구성도
![Image](https://github.com/user-attachments/assets/024be721-b7c5-4ae2-8a08-06d7da6ed3fc)

---

## 🔧 사용 기술

### MCU
- **Raspberry Pi**: 메인 서버
- **STM32**: DHT11센서를 통한 온습도 데이터 수집
- **Arduino**: 릴레이 제어(가전제품 대용)

### 센서 및 모듈
- **DHT11**: 온습도 센서 (STM32와 연결)
- **릴레이**: 릴레이 ON/OFF를 통해 연결된 장치 제어 (Arduino와 연결)
- **Wi-Fi 모듈**: STM32와 연결되어 서버와 통신
- **블루투스 모듈**: Arduino와 연결되어 서버와 통신
- **LCD**: STM32에서 온습도 데이터 시각화

### 소프트웨어
- **TCP/IP 통신**: 각 보드간 소켓 통신을 통한 데이터 송수신
- **Grafana**: 시각화를 위한 데이터 대시보드 (다른 팀원이 담당)
- **앱 연동**: TCP Telnet Terminal앱을 이용해 모바일 환경에서 제어
---

## 🛠️ 트러블슈팅

- **문제**: 온습도가 임계점 부근에서 명령이 반복적으로 보내지는 문제 발생
- **해결**: 초기에는 조건문에 이전 상태를 비교하는 조건을 추가하는 2점 제어를 통해 해결 시도했으나 여전히 문제 발생
   -> 상태 변경 범위를 넓혀 줌으로써 문제 해결

```c
if (curState == MANUAL) continue;
if (humi == STOP && atoi(pArray[1]) < humiLow - 1) {
    humi = RUN;
} else if (humi == RUN && atoi(pArray[1]) >= humiHigh + 1) {
    humi = STOP;
}
```
---

