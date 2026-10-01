# Raspberry Pi Edge Vision

**개발 기간:** 2026.09.21 ~ 2026.09.28

**Jetson 버전:** 기존 파이프라인을 Jetson으로 이식하고 TensorRT 기반 GPU 추론을 적용한 [Jetson Edge Vision](https://github.com/triton0305/jetson-edge-vision)을 개발 중입니다.

Raspberry Pi 4에서 USB Webcam 영상을 YOLO26n ONNX로 처리해 차량 Detection을 생성하는 C++17 Vision Client입니다. 탐지 객체마다 `vision` JSON을 생성하고, 별도 네트워크 스레드를 통해 TCP/ACK 방식으로 전달합니다.

독립 검증용 [Relay Server](https://github.com/triton0305/pi-edge-vision-relay-server)를 구현해 Raspberry Pi Client → TCP → Relay Server → SQLite → ACK 전체 흐름을 실제 환경에서 검증했습니다.

## Key Features

- `car`, `motorcycle`, `bus`, `truck` 탐지 및 Class-aware NMS
- Detection 1개당 `vision` JSON과 `message_id` 각각 1개 생성
- 영상 처리와 네트워크 송신을 분리한 Message Queue / Network Worker 구조
- 유한 Queue를 사용한 Producer-Consumer 구조
- 4-byte big-endian length-prefix 기반 TCP 프로토콜
- Partial read/write 처리
- ACK 검증, Timeout, Retry 및 자동 재연결
- 재전송 시 동일 `message_id`와 payload 유지
- FPS, 추론 시간, Queue 크기 및 Drop 수 측정
- 개발 환경과 운영 Runtime을 분리한 `/opt`, `/var/lib` 배포 구조

## Architecture

| Stage | Flow |
|---|---|
| **① Vision Loop** | USB Webcam (V4L2) → Letterbox → YOLO26n ONNX → Class-aware NMS |
| **② Message Generation** | Detection → 객체별 `vision` JSON → Message Queue |
| **③ Delivery** | Network Worker → TCP → ACK / Retry |
| **④ Relay Server** | JSON 검증 → SQLite `detections` 저장 → ACK 반환 |

현재 Runtime의 핵심 흐름은 다음과 같습니다.

```text
USB Webcam
    ↓
Preprocessor
    ↓
YOLO26n ONNX
    ↓
PostProcessor
    ↓
Detection
    ↓
Serializer
    ↓
Message Queue
    ↓
Network Worker
    ↓
TCP / ACK
    ↓
Relay Server
    ↓
SQLite
```

## Message Protocol

TCP 메시지는 다음 형식을 사용합니다.

```text
4-byte big-endian payload length
+
JSON payload
```

`timestamp_ms`는 프레임 획득 직후 생성한 Unix timestamp(ms)이며, bbox는 원본 640×480 프레임 기준 픽셀 좌표입니다.

```json
{
  "version": 1,
  "type": "vision",
  "device_id": "vision-pi-01",
  "message_id": "vision-pi-01-000059-00000001",
  "data": {
    "frame_id": 0,
    "timestamp_ms": 1790580489875,
    "class_id": 2,
    "class_name": "car",
    "confidence": 0.2803,
    "bbox": {
      "x": 131,
      "y": 328,
      "width": 85,
      "height": 70
    }
  }
}
```

서버는 동일한 length-prefix 형식으로 ACK를 반환합니다.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "vision-pi-01-000059-00000001",
  "status": "ok"
}
```

## Reliability and Network Behavior

Client는 ACK의 `message_id`와 상태를 검증합니다.

일반 `vision` 메시지는 송신 또는 ACK 수신에 실패하면 최초 전송을 포함해 최대 3회 시도합니다. 재전송할 때는 동일한 `message_id`와 payload를 유지합니다.

네트워크 연결이 실패해도 Vision Loop는 종료하지 않으며, 별도 Network Worker가 재연결을 시도합니다. 재시도 한도를 넘은 미전송 메시지는 Drop 처리합니다.

Server Error ACK 또는 유효하지 않은 ACK는 정상적인 네트워크 장애와 구분하여 실행 실패로 처리합니다.

## Client / Server Responsibility

| Component | Responsibility |
|---|---|
| **Vision Client** | 프레임 획득, 전처리, 추론, 후처리, 객체별 Detection 생성, JSON 직렬화, Queue 및 TCP/ACK 전달 |
| **Relay Server** | `vision` 메시지 검증, SQLite `detections` 저장, 동일 `message_id` 중복 방지, ACK 반환 |
| **Server-side Analysis** | 저장된 Detection을 `timestamp_ms` 기준으로 조회·집계하는 후처리 |

Vision Client는 원본 Detection 생성과 안정적인 전달에 집중합니다.

5초·1분·시간대별 Detection 건수, 차종별 탐지 건수와 비율, confidence 필터링, 시간 구간 조회, 시각화 및 데이터 보관 정책은 저장된 원본 데이터를 이용한 서버 측 후처리 영역입니다.

현재 Relay Server는 원본 저장과 ACK까지 구현되어 있으며, 통계 조회·시각화·보관 정책은 현재 Relay Server Runtime의 구현 범위에 포함하지 않습니다.

## Development Environment

| Item | Environment |
|---|---|
| Board / OS | Raspberry Pi 4 / Debian GNU/Linux 13, 64-bit |
| Camera | USB Webcam, OpenCV V4L2, 640×480, 30 FPS 요청 |
| Language | C++17 |
| Build | CMake |
| Inference | OpenCV 4.10.0 DNN / CPU |
| Model | YOLO26n ONNX |
| Model Input | 640×640 |
| JSON | nlohmann/json |
| Network | TCP / `std::thread` |

대상 COCO Class는 다음과 같습니다.

- `car(2)`
- `motorcycle(3)`
- `bus(5)`
- `truck(7)`

## Performance

Raspberry Pi 4 CPU 환경에서 측정한 결과입니다. 실제 값은 장면과 실행 조건에 따라 달라질 수 있습니다.

| Metric | Result |
|---|---:|
| Camera Frame Grab | 약 21.6 FPS |
| Effective FPS | 약 2.2–2.34 FPS |
| Inference Latency | 약 405–425 ms |

## Engineering Decisions & Troubleshooting

### Reliable TCP Delivery

TCP는 메시지 경계를 보장하지 않으므로 JSON payload 앞에 4-byte big-endian length-prefix를 추가하고, partial read/write에 대응하도록 송수신을 구현했습니다.

ACK 유실로 동일 Detection이 재전송될 수 있기 때문에 Retry 시 새로운 메시지를 생성하지 않고 동일한 `message_id`와 payload를 유지합니다. Relay Server에서는 `message_id`를 idempotency key로 사용하여 중복 저장을 방지합니다.

```text
Detection
→ JSON
→ 4-byte Length Prefix
→ TCP
→ Server Validation / SQLite
→ ACK

ACK Timeout / Disconnect
→ Reconnect
→ Same message_id / Same payload Retry
```

동일 `message_id`와 동일 payload가 다시 도착하면 정상적인 Retry로 처리하고, 동일 `message_id`에 다른 데이터가 들어오면 `MESSAGE_ID_CONFLICT`로 구분합니다. ACK는 DB 반영 또는 기존 데이터 확인 이후 전송하도록 하여 ACK 성공과 실제 저장 상태가 일치하도록 구성했습니다.

### Vision / Network Fault Isolation

초기에는 Client 시작 시 Relay Server에 연결할 수 없으면 Vision Runtime까지 종료될 수 있었습니다. Camera와 YOLO 동작이 Network 상태에 종속되지 않도록 Vision 처리와 Network 전송을 분리했습니다.

```text
Vision Worker
Camera → YOLO → Detection
             ↓
       Message Queue
             ↓
       Network Worker
             ↓
      TCP / ACK / Retry
```

Relay Server가 실행되지 않았거나 연결이 끊어진 경우에도 Vision 처리는 계속되며, Network Worker가 별도로 재연결을 시도합니다. 이를 통해 Network 장애가 Camera 및 Inference 동작 전체로 전파되지 않도록 했습니다.

### Traffic Counting Experiment and Scope Redesign

반복되는 Detection 데이터를 줄이고 차량 통과량을 계산하기 위해 Tracker와 Line Crossing 기반의 `traffic_count`를 실험적으로 구현했습니다.

```text
Detection
→ Tracker
→ track_id
→ Line Crossing
→ 5-second traffic_count
```

실제 Raspberry Pi 환경에서 검증한 결과, 제한된 Camera FOV와 낮은 처리 FPS 환경에서는 연속 Tracking과 Line Crossing 조건을 안정적으로 유지하기 어려웠고 `traffic_count`가 기대한 형태로 생성되지 않았습니다.

이를 계기로 Detection 데이터와 Traffic Count가 서로 다른 의미를 가진다는 점을 다시 검토했습니다. 또한 시간 구간이나 집계 방식이 변경될 때마다 Edge Client의 로직까지 변경하는 구조보다, Client는 원본 Detection을 전달하고 저장된 데이터를 기반으로 필요한 통계를 후처리하는 구조가 확장에 더 적합하다고 판단했습니다.

최종 Runtime에서는 Tracker, Line Crossing, `traffic_count`를 제외하고 다음과 같이 구조를 단순화했습니다.

```text
Raspberry Pi Client
Camera
→ YOLO Detection
→ vision JSON
→ Message Queue
→ TCP / ACK

Relay Server
vision 저장
→ timestamp_ms 기반 후처리
→ 시간 구간 / 차종별 Detection 통계
```

이를 통해 Edge Client는 Detection과 전송에 집중하고, 집계 기준의 변경은 저장된 Detection 데이터를 활용할 수 있도록 구성했습니다.

## Build

### Requirements

빌드 및 실행에는 다음 환경이 필요합니다.

- C++17 compiler
- CMake
- OpenCV
  - core
  - imgproc
  - imgcodecs
  - highgui
  - videoio
  - dnn
- nlohmann/json
- YOLO26n ONNX model

YOLO26n ONNX 모델 파일은 Repository에 포함하지 않으며 별도로 준비해야 합니다.

개발 환경의 기본 모델 경로는 다음과 같습니다.

```text
models/yolo26n.onnx
```

Repository 루트에서 빌드합니다.

```bash
cmake -S . -B build
cmake --build build -j
```

빌드가 완료되면 실행 파일은 다음 위치에 생성됩니다.

```text
build/bin/edge_vision
```

## Development Run

개발 환경에서는 Repository 루트에서 다음과 같이 실행합니다.

```bash
./build/bin/edge_vision <server_ip> <server_port>
```

기본 개발 경로는 다음과 같습니다.

```text
Model
└── models/yolo26n.onnx

Boot ID
└── boot_id.dat
```

현재 Runtime은 OpenCV HighGUI를 통해 Detection 화면을 표시하므로 GUI를 사용할 수 있는 환경에서 실행해야 합니다.

## Deployment

운영 환경에서는 개발 Repository와 Runtime 파일을 분리합니다.

```text
/opt/edge_vision/
├── bin/
│   └── edge_vision
└── models/
    └── yolo26n.onnx

/var/lib/edge_vision/
└── boot_id.dat
```

운영 경로를 지정해 별도의 Deployment Build를 생성합니다.

```bash
cmake -S . -B build-deploy \
  -DMODEL_PATH=/opt/edge_vision/models/yolo26n.onnx \
  -DBOOT_ID_PATH=/var/lib/edge_vision/boot_id.dat

cmake --build build-deploy -j
```

빌드한 실행 파일을 운영 경로에 배치합니다.

```bash
sudo install -m 755 \
  build-deploy/bin/edge_vision \
  /opt/edge_vision/bin/edge_vision
```

운영 전용 계정 `edgevision`은 USB Webcam에 접근할 수 있어야 하며 `/var/lib/edge_vision/boot_id.dat`에 대한 쓰기 권한이 필요합니다.

### Production Run

Repository 루트의 `pirun`을 사용해 운영 Runtime을 실행합니다.

```bash
./pirun <server_ip> <server_port>
```

`pirun`은 내부적으로 다음 실행 파일을 `edgevision` 계정으로 실행하면서 Server IP와 Port를 전달합니다.

```bash
sudo -u edgevision \
  /opt/edge_vision/bin/edge_vision \
  <server_ip> <server_port>
```

Relay Server의 빌드 및 실행 방법은 [Edge Vision Relay Server](https://github.com/triton0305/pi-edge-vision-relay-server)를 참고하세요.

## Project Structure

```text
.
├── include/
│   ├── core/
│   ├── vision/
│   ├── protocol/
│   └── network/
├── src/
│   ├── core/
│   ├── vision/
│   ├── protocol/
│   ├── network/
│   └── main.cpp
├── test/
├── CMakeLists.txt
└── pirun
```

각 디렉터리의 역할은 다음과 같습니다.

- `vision`
  - Camera
  - Preprocessing
  - YOLO Inference
  - Post-processing
- `protocol`
  - JSON 직렬화
  - 송신 메시지 형식
- `network`
  - Message Queue
  - TCP Client
  - ACK 검증
  - Retry / Reconnect
- `core`
  - Config
  - Boot ID
  - Message ID
  - Metrics
- `main.cpp`
  - 컴포넌트 초기화
  - Thread 시작 및 종료
  - Runtime Lifecycle 관리

## Scope and Data Semantics

현재 Runtime의 데이터 의미와 기능 범위는 다음과 같습니다.

- Detection이 없는 프레임은 `vision` 메시지를 생성하지 않습니다.
- 동일 차량이 여러 추론 프레임에서 반복 탐지되면 각각 별도의 Detection 이력으로 저장합니다.
- Detection 건수는 실제 고유 차량 대수를 의미하지 않습니다.
- Detection 건수는 기준선 통과 차량 수나 실제 누적 교통량을 의미하지 않습니다.
- Detection 결과만으로 도로 전체의 절대적인 혼잡도를 의미한다고 해석하지 않습니다.
- 시간 구간별 통계는 Server가 저장된 `timestamp_ms`를 기준으로 계산합니다.
- 네트워크 지연이나 Retry로 늦게 도착한 데이터도 Server 수신 시각이 아닌 원래 `timestamp_ms`가 속한 시간 구간에 반영합니다.
- Tracking, Line Crossing, `traffic_count`, `traffic_state`는 현재 Runtime에 포함하지 않습니다.

차종별 Detection 비율을 계산할 경우 다음 기준을 사용합니다.

```text
특정 차종 Detection 건수
÷
전체 차량 Detection 건수
×
100
```

전체 차량 Detection은 `car`, `motorcycle`, `bus`, `truck` Detection 건수의 합이며, 동일 차량의 반복 Detection도 포함합니다.

분자와 분모에는 동일한 시간 구간과 confidence 필터를 적용하며, 전체 Detection 건수가 0인 경우 비율은 데이터 없음으로 처리합니다.

해당 비율은 실제 도로의 차량 구성비가 아니라 Detection 결과 기준 통계입니다.

## Integration Verification

다음 E2E 흐름을 실제 환경에서 검증했습니다.

```text
USB Webcam
→ Raspberry Pi Client
→ TCP
→ Windows / WSL Relay Server
→ SQLite
→ ACK
→ Raspberry Pi Client
```

최종 Vision-only 테스트에서는 객체별 `vision` 메시지가 Relay Server의 SQLite `detections` 테이블에 저장되고 정상 ACK가 반환되는 것을 확인했습니다.

또한 다음 동작을 별도로 검증했습니다.

- 동일 `message_id` 재전송 시 DB 중복 행 방지
- ACK `ok` 처리
- Error ACK 처리
- ACK Timeout 후 Retry
- 연결 종료 후 재연결
- Server 미실행 상태에서도 Vision Loop 계속 실행

## Traffic Feature Decision

개발 과정에서 Tracking과 Line Crossing 기반의 5초 `traffic_count` 기능까지 구현하고 실제 E2E 테스트를 진행했습니다.

이후 실제 테스트 결과와 데이터 활용 목적을 다시 검토하여 최종 Runtime의 역할을 **객체별 Detection 이력 생성 및 안정적인 전달**로 확정했습니다.

현재 저장소에는 Tracker 및 Traffic Counting 관련 일부 소스가 남아 있지만, 현재 Vision Runtime의 실행 경로에서는 호출하지 않습니다.

`traffic_state` 역시 대안으로 검토했으나 현재 기능 범위에는 포함하지 않습니다.

## Future Extensions

- systemd 기반 자동 실행 및 장애 시 재시작
- Camera 장애 복구
- 장시간 Runtime 안정성 검증
- 장기 네트워크 장애 상황의 Queue / Drop 정책 보완
- Server-side Detection 통계 조회
- 시각화
- 데이터 보관 및 삭제 정책
