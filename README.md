# Raspberry Pi Edge Vision

Raspberry Pi / Embedded Linux 환경에서 USB Webcam 영상을 입력받아 YOLO26n으로 차량을 탐지하고, 탐지 결과를 TCP 서버로 전달하는 Edge Vision 시스템입니다.

YOLO26n을 ONNX 형식으로 변환해 OpenCV DNN으로 추론하며, 영상 처리와 Network 처리를 분리한 C++ 기반 Pipeline을 구성했습니다.

현재 Raspberry Pi Vision Client에서 실제 Relay Server로 Detection 데이터를 전송하고, ACK 검증 후 SQLite에 저장되는 전체 End-to-End Pipeline까지 검증한 상태입니다.

---

## Detection Pipeline

```text
Pleomax W-210 USB Webcam
    ↓
Frame Capture (OpenCV + V4L2)
    ↓
Letterbox 640 × 640
    ↓
YOLO26n ONNX Inference
    ↓
Coordinate Restore + Clipping
    ↓
Vehicle Filtering + Class-aware NMS
    ↓
Object-based JSON
    ↓
MessageQueue
    ↓
Network Worker
    ↓
4-byte Length-Prefix TCP
    ↓
Relay Server
    ├─ SQLite vision_data
    └─ ACK Response
         ↓
Client ACK Validation / Timeout / Retry
```

COCO 클래스 중 차량에 해당하는 객체만 탐지 대상으로 사용합니다.

| Class ID | Class      |
| -------: | ---------- |
|        2 | car        |
|        3 | motorcycle |
|        5 | bus        |
|        7 | truck      |

Confidence Threshold `0.25`, NMS Threshold `0.45`를 적용합니다.

---

## Vision Processing

USB Webcam의 `640 × 480` 프레임을 종횡비를 유지한 채 `640 × 640` Letterbox로 변환하고, Raspberry Pi CPU에서 YOLO26n ONNX 추론을 수행합니다.

YOLO 출력 좌표는 Letterbox에 적용된 Scale과 Padding을 역산해 원본 좌표계로 복원합니다. 이후 Bounding Box를 이미지 범위로 Clipping하고 차량 Class Filtering과 Class-aware NMS를 적용합니다.

```text
640 × 480 Frame
    ↓
Letterbox 640 × 640
    ↓
YOLO26n ONNX
    ↓
Original Coordinate Restore
    ↓
Bounding Box Clipping
    ↓
Vehicle Filtering
    ↓
Class-aware NMS
```

각 프레임에는 `frame_id`와 Frame Capture 시점의 Unix timestamp(ms)인 `timestamp_ms`를 부여합니다.

---

## Message Protocol

한 프레임에서 여러 객체가 검출되더라도 각 Detection을 독립된 메시지로 처리합니다.

```text
Frame 32
├─ car   → Message A
├─ truck → Message B
└─ bus   → Message C
```

같은 프레임의 객체는 `frame_id`와 `timestamp_ms`가 같을 수 있지만 각각 고유한 `message_id`를 가집니다. 탐지된 객체가 없는 프레임은 전송하지 않습니다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000029-00000006",
  "data": {
    "frame_id": 33,
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
Detection 1개
= JSON Message 1개
= message_id 1개
= ACK 1개
```

후단 SQLite 저장도 객체 1개를 하나의 Row로 처리하는 구조를 기준으로 합니다.

### Message ID

```text
<device_id>-<boot_id>-<sequence>
```

예:

```text
vision-pi-01-000029-00000006
```

| Field       | Role               |
| ----------- | ------------------ |
| `device_id` | 장치 식별              |
| `boot_id`   | Client 실행 세션 식별    |
| `sequence`  | 실행 중 생성된 객체 메시지 순번 |

`boot_id`는 로컬 파일에 영속화하고 Client 실행 시 증가시킵니다. `sequence`는 실행마다 다시 시작합니다.

이를 통해 Client 재시작 후 `frame_id`와 `sequence`가 초기화되어도 이전 메시지와 ID가 충돌하지 않도록 구성했습니다.

---

## Threaded Network Pipeline

Vision Loop가 ACK 응답 시간에 직접 영향을 받지 않도록 Vision 처리와 Network 처리를 분리했습니다.

```text
Vision Worker
    │
    ▼
MessageQueue
    │
    ▼
Network Worker Thread
    ├─ TCP Send
    ├─ ACK Receive
    ├─ ACK Validation
    └─ Retry / Reconnect
```

Vision과 Network 사이에는 Thread-safe bounded queue를 사용합니다.

```text
Maximum Queue Size : 16
Overflow Policy    : Drop Oldest
```

Queue가 가득 차면 가장 오래된 메시지를 제거해 최신 Detection을 유지하며, Queue Size와 Dropped Count를 Metrics로 확인합니다.

`SIGINT`와 `SIGTERM`도 처리해 Vision Loop 종료 → Queue Close → Network Thread Join → Socket / Camera 해제 순서로 종료합니다.

---

## TCP Reliability

TCP Stream의 메시지 경계를 복원하기 위해 JSON Payload 앞에 4-byte Length Prefix를 추가합니다.

```text
┌───────────────────────┬────────────────────────────┐
│ 4-byte Payload Length │        JSON Payload        │
└───────────────────────┴────────────────────────────┘
        big-endian                  N bytes
```

Partial Read / Write를 고려해 `sendAll()`과 `readAll()`에서 필요한 길이를 모두 처리할 때까지 반복 송수신하며, 송신에는 `MSG_NOSIGNAL`을 사용해 연결 종료 시 `SIGPIPE`를 방지합니다.

Server는 정상 처리한 메시지에 동일한 `message_id`를 포함한 ACK를 반환합니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000029-00000006",
  "status": "ok"
}
```

Client는 `version`, `type`, `message_id`, `status`를 검증하고 `status: "error"` 응답은 `error_code`를 통해 구분합니다.

ACK Timeout은 `1500 ms`, 최대 Retry 횟수는 `2회`입니다. ACK Timeout 또는 수신 실패 시 연결을 다시 시도한 뒤 동일한 Payload와 `message_id`로 재전송합니다.

```text
Initial Send
    ↓
ACK Wait (1.5 s)
    ↓
ACK OK ───────────→ Complete
    │
    └─ Timeout / Receive Failure
              ↓
           Reconnect
              ↓
      Same Payload / message_id
              ↓
            Retry
```

최초 전송을 포함해 최대 3회의 전송 기회를 가집니다.

---

## Performance

Raspberry Pi 4 CPU 환경에서 Pipeline을 실행하며 성능과 Queue 상태를 측정했습니다.

| Metric              |        Measured |
| ------------------- | --------------: |
| Effective FPS       | ~2.2 – 2.34 FPS |
| Inference Latency   |   ~405 – 425 ms |
| Delivery Latency    |   ~100 – 130 ms |
| Observed Queue Size |       Max 3 → 0 |
| Dropped Messages    |               0 |

Network 처리를 별도 Worker로 분리한 상태에서 Vision Loop가 약 `2.2 ~ 2.3 FPS` 수준으로 유지됐으며, 테스트 구간에서는 Queue가 정상적으로 소진되고 Message Drop이 발생하지 않았습니다.

> 측정값은 현재 Raspberry Pi 4 / CPU inference 테스트 환경 기준입니다.

---

## Relay Server Integration

초기 통신 기능은 개발 PC의 C Dummy Server로 검증한 뒤 실제 Relay Server와 연동했습니다.

Client는 Server IP와 Port를 실행 인자로 전달받습니다.

```bash
./bin/edge_vision <server_ip> <server_port>
```

실제 Relay Server 테스트에서 연속 Detection Message에 대한 ACK를 확인했습니다.

```text
vision-pi-01-000029-00000004 → ACK OK
vision-pi-01-000029-00000005 → ACK OK
vision-pi-01-000029-00000006 → ACK OK
```

현재 **Raspberry Pi Vision Client ↔ Relay Server 간 Detection 전송 및 TCP + ACK End-to-End 통신까지 실제 장비에서 검증**했습니다.

Relay Server가 수신한 Detection Message가 SQLite `vision_data`에 저장되는 것까지 확인했습니다.

---

## Project Structure

소스는 기능 책임을 기준으로 `core / vision / protocol / network` 영역으로 분리했습니다.

```text
include/
├── core/
├── vision/
├── protocol/
└── network/

src/
├── core/
├── vision/
├── protocol/
├── network/
└── main.cpp
```

| Area       | Role                                                        |
| ---------- | ----------------------------------------------------------- |
| `core`     | Config, Boot ID, Message ID, DetectionResult, Metrics       |
| `vision`   | Camera, Preprocessor, Detector, PostProcessor, VisionWorker |
| `protocol` | JSON Serialization, Outbound Message                        |
| `network`  | TCP Client, ACK, MessageQueue, NetworkWorker                |
| `main.cpp` | 객체 생성, 초기화, 실행 및 종료 흐름                                      |

`main.cpp`는 세부 기능 대신 각 모듈의 초기화와 실행 Lifecycle을 관리합니다.

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

| Item               | Value                                    |
| ------------------ | ---------------------------------------- |
| Language           | C++17                                    |
| Build              | CMake                                    |
| Vision / Inference | OpenCV / OpenCV DNN                      |
| Inference Target   | CPU                                      |
| Model              | YOLO26n ONNX                             |
| Model Input        | 640 × 640                                |
| Serialization      | nlohmann/json                            |
| Network            | TCP/IP                                   |
| Threading          | `std::thread`, Mutex, Condition Variable |

> Camera의 30 FPS는 설정 요청값이며, 실제 Frame Grab 성능은 테스트 환경에서 약 21.6 FPS로 확인했습니다.

---

## Current Status

Vision / Network 비동기 처리, 객체 단위 메시지 프로토콜, Persistent Boot ID 기반 Message ID, bounded queue, ACK / Timeout / Retry / Reconnect, Graceful Shutdown을 구현했습니다.

현재 Raspberry Pi Vision Client에서 실제 Relay Server로 Detection 데이터를 전송하고, ACK 검증 후 SQLite에 저장되는 전체 End-to-End Pipeline까지 검증한 상태입니다.

---

## Next

* Detection Visualization + FPS / Latency Overlay
* ROI 기반 제한구역 판정 및 관제 Snapshot
* 운영 환경 보안 및 안정성 보강
* Jetson / TensorRT 기반 성능 확장
