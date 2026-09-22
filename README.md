# Raspberry Pi Edge Vision

Raspberry Pi / Embedded Linux 환경에서 USB Webcam 영상을 실시간으로 처리하고, YOLO26n 기반 차량 탐지 결과를 TCP 서버로 전달하는 Edge Vision 시스템입니다.

사전학습된 YOLO26n 모델을 ONNX 형식으로 변환해 OpenCV DNN으로 추론하며, 영상 입력과 후처리, 탐지 결과 구조화, TCP 통신을 C++ 기반으로 구성했습니다.

현재 Raspberry Pi에서 생성한 차량 탐지 데이터를 개발 PC의 C 기반 Dummy Server로 전송하고, 객체별 JSON 수신과 ACK 응답까지 확인한 상태입니다.

---

## Detection Pipeline

```text
USB Webcam
    ↓
OpenCV / V4L2 Frame Capture
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
ACK
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

모델 검증에는 Ultralytics를 사용하고, Raspberry Pi에서 동작하는 실제 Client는 YOLO26n ONNX 모델과 OpenCV DNN을 이용해 C++로 구성했습니다.

USB Webcam 입력은 OpenCV의 V4L2 backend를 사용하며, 입력 프레임을 전처리한 뒤 CPU에서 추론합니다.

### Detection Processing

YOLO 출력에서 차량 클래스의 `class_id`, `class_name`, `confidence`, `bbox`를 추출하고 NMS를 적용합니다.

각 프레임에는 `frame_id`와 Frame Capture 시점의 Unix timestamp(ms)인 `timestamp_ms`를 부여합니다.

한 프레임에서 여러 객체가 검출될 수 있지만, 네트워크에서는 각 객체를 독립된 메시지로 처리합니다.

```text
Frame 32
├─ car   → Message 1
├─ truck → Message 2
└─ bus   → Message 3
```

따라서 같은 프레임의 객체들은 `frame_id`와 `timestamp_ms`가 같을 수 있지만 각각 다른 `message_id`를 가집니다.

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

후단 SQLite 연동 시에도 하나의 객체를 하나의 Row로 저장하는 구조를 기준으로 합니다.

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
| `boot_id`   | 프로그램 실행 세션 식별     |
| `sequence`  | 해당 실행에서 전송한 객체 순번 |

`boot_id`는 로컬 파일에 저장하고 프로그램 시작 시 증가시킵니다. `sequence`는 실행마다 다시 시작해 객체를 전송할 때마다 증가합니다.

이 구조를 사용해 프로그램 또는 Raspberry Pi가 재시작되어 `frame_id`가 다시 초기화되더라도 과거 메시지와 ID가 충돌하지 않도록 했습니다.

---

## TCP Framing

TCP Stream에는 메시지 경계가 없기 때문에 JSON Payload 앞에 4-byte 길이 정보를 추가합니다.

```text
┌───────────────────────┬────────────────────────────┐
│ 4-byte Payload Length │        JSON Payload        │
└───────────────────────┴────────────────────────────┘
        big-endian                  N bytes
```

Client는 JSON Payload 크기를 `uint32_t` Network Byte Order로 변환해 먼저 전송하고, 이후 실제 Payload를 전송합니다.

송수신 과정에서는 한 번의 `send()` 또는 `recv()`로 전체 데이터가 처리된다고 가정하지 않고 필요한 길이만큼 반복 처리합니다.

---

## ACK

Dummy Server는 메시지를 정상적으로 수신하면 동일한 `message_id`를 포함한 ACK를 반환합니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000009-00000001",
  "status": "ok"
}
```

이를 통해 Client가 전송한 객체와 Server가 처리한 객체를 `message_id` 기준으로 대응시킬 수 있습니다.

현재 ACK 송수신까지 확인했으며, ACK Timeout과 동일 `message_id`를 이용한 Retry 처리는 다음 단계로 남겨두었습니다.

---

## End-to-End Validation

개발 PC에 C 기반 Dummy Server를 구성해 Raspberry Pi와 실제 TCP 통신을 검증했습니다.

Server에서 실제 수신한 Payload:

```text
Payload size: 266 bytes

{"data":{"bbox":{"height":229,"width":273,"x":242,"y":84},"class_id":2,"class_name":"car","confidence":0.27056142687797546,"frame_id":32,"timestamp_ms":1790043017712},"device_id":"vision-pi-01","message_id":"vision-pi-01-000009-00000001","type":"vision","version":1}
```

Server ACK:

```text
ACK sent: {"version":1,"type":"ack","message_id":"vision-pi-01-000009-00000001","status":"ok"}
```

현재 다음 구간까지 실제 동작을 확인했습니다.

```text
USB Webcam → YOLO26n → Vehicle Detection
→ Object-based JSON → Length-Prefix TCP
→ PC Dummy Server → ACK
```

TCP Server의 IPv4 주소와 Port는 `TcpClient` 생성 시 지정합니다.

```cpp
TcpClient tcp_client("SERVER_IP", 5000);
```

따라서 네트워크 처리 코드를 변경하지 않고 연결 대상만 변경할 수 있습니다.

---

## Design

### Object-based Message

초기에는 한 프레임의 여러 Detection을 배열로 묶어 전송하는 방식을 검토했지만, Server와 DB의 처리 단위를 맞추기 위해 객체 단위 메시지 구조로 변경했습니다.

```text
Frame
├─ Detection A → JSON A → message_id A → ACK A
├─ Detection B → JSON B → message_id B → ACK B
└─ Detection C → JSON C → message_id C → ACK C
```

객체 단위로 식별자를 부여해 향후 SQLite 저장과 재전송도 동일한 단위로 처리할 수 있도록 했습니다.

### Vision / Network Separation

Vision 처리 결과와 Socket 처리를 직접 결합하지 않고 탐지 결과 구조화와 직렬화 단계를 분리했습니다.

```text
PostProcessor
    ↓
Detection Result
    ↓
Serializer
    ↓
TcpClient
```

`main.cpp`는 전체 실행 흐름을 연결하고, Camera 입력, 추론, 후처리, 직렬화, TCP 통신은 각각의 모듈이 담당합니다.

### Client / Server Separation

Edge Vision Client는 영상 처리와 탐지 결과 전송을 담당하고, Server / DB 영역은 TCP Interface를 기준으로 분리했습니다.

현재는 실제 후단 시스템 연동 전에 개발 PC의 C Dummy Server를 이용해 Client의 End-to-End 전송과 ACK 수신을 검증했습니다.

---

## Project Structure

| Module            | Role                            |
| ----------------- | ------------------------------- |
| `Camera`          | USB Webcam / V4L2 Frame Capture |
| `Preprocessor`    | YOLO 입력 전처리                     |
| `Detector`        | YOLO26n ONNX 추론                 |
| `PostProcessor`   | 차량 필터링, Bounding Box 처리, NMS    |
| `DetectionResult` | Frame / Detection 데이터 구조        |
| `Serializer`      | 객체별 JSON 직렬화                    |
| `TcpClient`       | TCP 연결 및 Length-Prefix 송수신      |
| `main`            | 전체 Pipeline 실행 및 모듈 연결          |

---

## Environment

| Category       | Environment                      |
| -------------- | -------------------------------- |
| Target         | Raspberry Pi                     |
| OS             | Raspberry Pi OS / Linux          |
| Language       | C++17                            |
| Camera         | USB Webcam                       |
| Camera Backend | V4L2                             |
| Vision         | OpenCV / OpenCV DNN              |
| Model          | YOLO26n ONNX                     |
| Network        | TCP/IP                           |
| Serialization  | JSON                             |
| Build          | CMake                            |
| Test Server    | C Dummy Server on Development PC |

---

## Current Status

**USB Webcam 입력 → YOLO26n 추론 → 차량 탐지 → 객체별 JSON 생성 → Length-Prefix TCP 전송 → PC Dummy Server 수신 → ACK 응답**까지 구현 및 검증했습니다.

현재 후단 C Server / SQLite와의 실제 통합 및 ACK Timeout / Retry 처리를 다음 단계로 진행하고 있습니다.

## Next

* C Server / SQLite 실제 통합
* ACK Timeout / Retry
* TCP 연결 끊김 및 재연결 처리
* ROI 기반 제한구역 진입 판정
* AI / Network Thread 분리 및 Queue 적용
* Graceful Shutdown
* FPS / Inference Latency 측정
* CPU / RAM / Queue 상태 모니터링
