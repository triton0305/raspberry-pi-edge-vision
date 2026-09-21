#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "camera.hpp"
#include "detection_result.hpp"
#include "detector.hpp"
#include "postprocessor.hpp"
#include "preprocessor.hpp"
#include "serializer.hpp"
#include "tcp_client.hpp"

int main()
{
  const std::string model_path = "../models/yolo26n.onnx";

  Camera camera(0, 640, 480, 30);
  Preprocessor preprocessor(640, 640);
  Detector detector(model_path);
  PostProcessor postprocessor(0.25f, 0.45f);
  Serializer serializer;
  TcpClient tcp_client("127.0.0.1", 5000);

  if (!camera.open())
  {
    std::cerr << "Failed to open camera\n";
    return 1;
  }

  if (!detector.isLoaded())
  {
    std::cerr << "Failed to load model\n";
    return 1;
  }

  if (!tcp_client.connectToServer())
  {
    std::cerr << "Failed to connect to server\n";
    return 1;
  }

  std::cout << "Edge Vision loop started\n";
  std::cout << "Press Ctrl+C to quit\n";

  std::uint64_t frame_id = 0;

  while (true)
  {
    cv::Mat frame;

    if (!camera.read(frame))
    {
      std::cerr << "Failed to capture frame\n";
      break;
    }

    cv::Mat blob = preprocessor.process(frame);
    std::vector<cv::Mat> outputs = detector.infer(blob);

    std::vector<Detection> detections = postprocessor.process(
      outputs, frame.cols, frame.rows,
      preprocessor.inputWidth(), preprocessor.inputHeight());

    DetectionResult result;

    result.frame_id = frame_id++;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    result.detections = detections;

    std::string message = serializer.serialize(result);

    if (!message.empty())
    {
      std::cout << message;
      if (!tcp_client.sendData(message))
      {
        std::cerr << "Failed to send detection result\n";
        break;
      }
    }
  }

  tcp_client.disconnect();
  camera.release();

  std::cout << "Edge Vision loop stopped\n";

  return 0;
}