# Raspberry Pi Edge Vision

Raspberry Pi / Embedded Linux 환경에서 USB Webcam 영상을 실시간으로 처리하고, YOLO26n 기반 차량 탐지 결과를 TCP 서버로 전달하는 Edge Vision 시스템입니다.

사전학습된 YOLO26n 모델을 ONNX 형식으로 변환해 OpenCV DNN으로 추론하며, 영상 입력과 후처리, 탐지 결과 구조화, TCP 통신을 C++ 기반으로 구성했습니다.

1차 MVP에서는 Raspberry Pi에서 생성한 차량 탐지 데이터를 개발 PC의 C 기반 Dummy Server로 전송하고, 객체별 JSON 수신, ACK 검증, Timeout 및 Retry까지 End-to-End로 구현했습니다.

---

## Detection Pipeline

```text
Pleomax W-210 USB Webcam
    ↓
Frame Capture (OpenCV + V4L2)
    ↓
Pre-processing
    ↓
YOLO26n ONNX Inference
    ↓
Vehicle Filtering + NMS
    ↓
Detection Result
    ↓
JSON Serialization
    ↓
4-byte Length-Prefix TCP
    ↓
PC C Dummy Server
    ↓
ACK Validation
    ↓
Timeout / Retry
```

COCO 클래스 중 차량에 해당하는 객체만 탐지 대상으로 사용합니다.

| Class ID | Class      |
| -------: | ---------- |
|        2 | car        |
|        3 | motorcycle |
|        5 | bus        |
|        7 | truck      |

YOLO 후처리에는 Confidence Threshold `0.25`, NMS Threshold `0.45`를 적용합니다.

---

## Implementation

### YOLO26n ONNX Inference

모델 검증에는 Ultralytics를 사용하고, Raspberry Pi에서 동작하는 Client는 YOLO26n ONNX 모델과 OpenCV DNN을 이용해 C++로 구성했습니다.

USB Webcam 입력은 OpenCV의 V4L2 backend를 사용하며, `640 × 640` 입력으로 전처리한 뒤 CPU에서 추론합니다.

### Detection Processing

YOLO 출력에서 차량 클래스의 `class_id`, `class_name`, `confidence`, `bbox`를 추출하고 NMS를 적용합니다.

각 프레임에는 `frame_id`와 Frame Capture 시점의 Unix timestamp(ms)인 `timestamp_ms`를 부여합니다.

한 프레임에서 여러 객체가 검출될 수 있지만 네트워크에서는 각 객체를 독립된 메시지로 처리합니다.

```text
Frame 32
├─ car   → Message 1
├─ truck → Message 2
└─ bus   → Message 3
```

같은 프레임에서 검출된 객체는 `frame_id`와 `timestamp_ms`가 같을 수 있지만 각각 다른 `message_id`를 가집니다.

탐지된 객체가 없는 프레임은 전송하지 않습니다.

---

## Message Protocol

각 탐지 객체는 하나의 JSON 메시지로 직렬화합니다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000009-00000001",
  "data": {
    "frame_id": 32,
    "timestamp_ms": 1790043017712,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.270561,
    "bbox": {
      "x": 242,
      "y": 84,
      "width": 273,
      "height": 229
    }
  }
}
```

전송 단위는 다음 기준으로 통일했습니다.

```text
객체 1개
= JSON Message 1개
= message_id 1개
= ACK 1개
```

후단 SQLite 연동에서도 객체 하나를 하나의 Row로 저장하는 구조를 기준으로 합니다.

---

## Message ID

메시지 식별자는 다음 형식을 사용합니다.

```text
<device_id>-<boot_id>-<sequence>
```

예:

```text
vision-pi-01-000009-00000001
```

| Field       | Role              |
| ----------- | ----------------- |
| `device_id` | 장치 식별             |
| `boot_id`   | Client 실행 세션 식별   |
| `sequence`  | 해당 실행에서 전송한 객체 순번 |

`boot_id`는 로컬 파일에 저장해 실행 세션마다 증가시키고, `sequence`는 각 실행에서 다시 시작해 객체를 전송할 때마다 증가시킵니다.

이를 통해 Client가 재시작되어 `frame_id`가 초기화되더라도 이전 실행에서 생성된 메시지와 `message_id`가 충돌하지 않도록 했습니다.

---

## TCP Framing

TCP Stream에는 메시지 경계가 없기 때문에 JSON Payload 앞에 4-byte 길이 정보를 추가합니다.

```text
┌───────────────────────┬────────────────────────────┐
│ 4-byte Payload Length │        JSON Payload        │
└───────────────────────┴────────────────────────────┘
        big-endian                  N bytes
```

Client는 JSON Payload 크기를 `uint32_t` Network Byte Order로 변환해 먼저 전송한 뒤 실제 Payload를 전송합니다.

송수신 과정에서는 한 번의 `send()` 또는 `recv()`로 전체 데이터가 처리된다고 가정하지 않고 `sendAll()`과 `readAll()`을 통해 필요한 길이만큼 반복 처리합니다.

---

## ACK / Retry

Dummy Server는 메시지를 정상적으로 수신하면 동일한 `message_id`를 포함한 ACK를 반환합니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000009-00000001",
  "status": "ok"
}
```

Client는 ACK의 `version`, `type`, `message_id`, `status`를 검증합니다.

Server가 오류를 반환한 경우 `status: "error"`와 함께 전달되는 `error_code`를 확인해 정상 ACK와 구분합니다.

ACK 수신에는 `1.5초` Timeout을 적용했습니다.

```text
Message 전송
    ↓
ACK 대기 (1.5 s)
    ↓
ACK OK ─────────────→ 다음 객체
    │
    └─ Timeout
         ↓
   동일 message_id로 Retry
         ↓
   최대 2회 재전송
```

ACK가 Timeout되면 새로운 메시지를 생성하지 않고 **동일한 `message_id`와 동일 Payload를 유지한 채 최대 2회 재전송**합니다.

Retry 한도를 초과하거나 연결이 끊어진 경우 현재 Client 실행을 종료하도록 처리했습니다.

---

## End-to-End Validation

개발 PC에 C 기반 Dummy Server를 구성하고 Raspberry Pi와 실제 TCP 통신을 검증했습니다.

Server에서 실제 수신한 Payload:

```text
Payload size: 266 bytes

{"data":{"bbox":{"height":229,"width":273,"x":242,"y":84},"class_id":2,"class_name":"car","confidence":0.27056142687797546,"frame_id":32,"timestamp_ms":1790043017712},"device_id":"vision-pi-01","message_id":"vision-pi-01-000009-00000001","type":"vision","version":1}
```

Server ACK:

```text
ACK sent: {"version":1,"type":"ack","message_id":"vision-pi-01-000009-00000001","status":"ok"}
```

1차 MVP에서 실제 검증한 범위는 다음과 같습니다.

```text
USB Webcam
→ YOLO26n ONNX
→ Vehicle Detection
→ Object-based JSON
→ Length-Prefix TCP
→ PC C Dummy Server
→ ACK Validation
→ Timeout / Retry
```

TCP Server의 IPv4 주소와 Port는 `TcpClient` 생성 시 지정합니다.

```cpp
TcpClient tcp_client("SERVER_IP", 5000);
```

연결 대상이 변경되더라도 네트워크 처리 로직을 수정하지 않고 Server IP와 Port만 변경할 수 있습니다.

---

## Design

### Object-based Message

초기에는 한 프레임에서 탐지된 여러 객체를 배열로 묶어 전송하는 구조를 검토했지만, Server와 DB의 처리 단위를 맞추기 위해 객체별 메시지 구조로 변경했습니다.

```text
Frame
├─ Detection A → JSON A → message_id A → ACK A
├─ Detection B → JSON B → message_id B → ACK B
└─ Detection C → JSON C → message_id C → ACK C
```

객체마다 고유한 `message_id`를 부여해 ACK 확인과 Retry를 동일한 객체 단위로 처리합니다.

향후 SQLite 저장 역시 같은 단위를 사용하도록 프로토콜을 구성했습니다.

### Vision / Network Separation

Vision 처리와 Socket 처리를 직접 결합하지 않고 데이터 구조화와 직렬화 단계를 분리했습니다.

```text
PostProcessor
    ↓
Detection Result
    ↓
Serializer
    ↓
TcpClient
```

`main.cpp`는 전체 실행 흐름을 연결하고 Camera 입력, 추론, 후처리, 직렬화, TCP 통신은 각각의 모듈이 담당합니다.

### Client / Server Separation

Edge Vision Client는 영상 처리와 탐지 결과 전송을 담당하고, Server / DB 영역은 TCP Interface를 기준으로 분리했습니다.

1차 MVP에서는 실제 후단 Server 연동 전에 개발 PC의 C Dummy Server를 이용해 TCP 송수신과 ACK / Retry 동작을 검증했습니다.

---

## Project Structure

| Module            | Role                                            |
| ----------------- | ----------------------------------------------- |
| `Camera`          | USB Webcam / V4L2 Frame Capture                 |
| `Preprocessor`    | YOLO 입력 전처리                                     |
| `Detector`        | YOLO26n ONNX 추론                                 |
| `PostProcessor`   | 차량 필터링, Bounding Box 처리, NMS                    |
| `DetectionResult` | Frame / Detection 데이터 구조                        |
| `Serializer`      | 객체별 JSON 직렬화                                    |
| `TcpClient`       | Length-Prefix 기반 TCP 송수신, ACK Timeout 처리        |
| `Ack`             | ACK JSON 파싱 및 응답 검증                             |
| `Config`          | Device ID, Protocol Version, Timeout / Retry 설정 |
| `main`            | 전체 Pipeline 실행, Message ID 생성 및 Retry 제어        |

---

## Environment

### Hardware / Camera

| Item            | Value                                    |
| --------------- | ---------------------------------------- |
| Board           | Raspberry Pi 4                           |
| OS              | Raspberry Pi OS 64-bit                   |
| Storage         | 128GB SD                                 |
| Camera          | Pleomax W-210 USB Webcam (`/dev/video0`) |
| Pixel Count     | 300K pixels                              |
| Capture Setting | 640 × 480, 30 FPS                        |
| Actual Grab     | ~21.6 FPS                                |

### Software / Inference

| Item               | Value                            |
| ------------------ | -------------------------------- |
| Language           | C++17                            |
| Build              | CMake                            |
| Vision / Inference | OpenCV / OpenCV DNN              |
| Inference Target   | CPU                              |
| Model              | YOLO26n ONNX                     |
| Model Input        | 640 × 640                        |
| Network            | TCP/IP                           |
| Serialization      | JSON                             |
| Test Server        | C Dummy Server on Development PC |

> Camera의 30 FPS는 설정 요청값이며, 실제 Frame Grab 성능은 테스트 환경에서 약 21.6 FPS로 확인했습니다.

---

## 1st MVP Status

1차 MVP에서 다음 구간을 구현하고 Raspberry Pi와 개발 PC를 이용해 실제 동작을 확인했습니다.

```text
USB Webcam Capture
→ YOLO26n ONNX Inference
→ Vehicle Detection
→ Object-based JSON
→ Message ID
→ 4-byte Length-Prefix TCP
→ PC C Dummy Server
→ ACK Validation
→ Timeout / Retry
```

현재 **Vision Client의 실시간 탐지와 기본 신뢰성 처리를 포함한 TCP End-to-End Pipeline**까지 완료한 상태입니다.

---

## Next

* 실제 C Server / SQLite 통합
* TCP 연결 끊김 및 재연결 처리
* ROI 기반 제한구역 진입 판정
* AI / Network Thread 분리 및 Queue 적용
* Graceful Shutdown
* FPS / Inference Latency 측정
* CPU / RAM / Queue 상태 모니터링
