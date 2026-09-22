#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "camera.hpp"
#include "config.hpp"
#include "detection_result.hpp"
#include "detector.hpp"
#include "postprocessor.hpp"
#include "preprocessor.hpp"
#include "serializer.hpp"
#include "tcp_client.hpp"
#include "ack.hpp"

std::int64_t currentUnixTimeMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

bool loadAndIncrementBootId(std::uint64_t& boot_id)
{
  boot_id = 0;

  std::ifstream input(Config::BOOT_ID_PATH);

  if (input.is_open())
  {
    input >> boot_id;

    if (input.fail())
    {
      std::cerr << "Failed to read boot_id\n";
      return false;
    }
  }

  ++boot_id;

  std::ofstream output(Config::BOOT_ID_PATH, std::ios::trunc);

  if (!output.is_open())
  {
    std::cerr << "Failed to write boot_id\n";
    return false;
  }

  output << boot_id << '\n';

  return true;
}

std::string createMessageId(std::uint64_t boot_id, std::uint64_t sequence)
{
  std::ostringstream stream;

  stream << Config::DEVICE_ID << '-'
         << std::setfill('0') << std::setw(6) << boot_id << '-'
         << std::setfill('0') << std::setw(8) << sequence;

  return stream.str();
}

int main()
{
  const std::string model_path = "../models/yolo26n.onnx";

  Camera camera(0, 640, 480, 30);
  Preprocessor preprocessor(640, 640);
  Detector detector(model_path);
  PostProcessor postprocessor(0.25f, 0.45f);
  Serializer serializer;
  TcpClient tcp_client("10.10.16.3", 5000);

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

  std::uint64_t boot_id = 0;

  if (!loadAndIncrementBootId(boot_id))
  {
    std::cerr << "Failed to initialize boot_id\n";
    return 1;
  }

  std::uint64_t frame_id = 0;
  std::uint64_t sequence = 0;

  std::cout << "Edge Vision loop started\n";
  std::cout << "Boot ID: " << boot_id << '\n';
  std::cout << "Press Ctrl+C to quit\n";

  while (true)
  {
    cv::Mat frame;

    if (!camera.read(frame))
    {
      std::cerr << "Failed to capture frame\n";
      break;
    }

    const std::uint64_t current_frame_id = frame_id++;
    const std::int64_t timestamp_ms = currentUnixTimeMs();

    cv::Mat blob = preprocessor.process(frame);
    std::vector<cv::Mat> outputs = detector.infer(blob);

    std::vector<Detection> detections = postprocessor.process(
      outputs, frame.cols, frame.rows,
      preprocessor.inputWidth(), preprocessor.inputHeight());

    for (const Detection& detection : detections)
    {
      ++sequence;

      const std::string message_id = createMessageId(boot_id, sequence);

      DetectionResult result;
      result.frame_id = current_frame_id;
      result.timestamp_ms = timestamp_ms;

      std::string message = serializer.serialize(result, detection, message_id);

      if (!message.empty())
      {
        std::cout << message << '\n';

        if (!tcp_client.sendData(message))
        {
          std::cerr << "Failed to send detection result\n";
          tcp_client.disconnect();
          camera.release();
          return 1;
        }

        std::string ack_message;

        if (!tcp_client.receiveData(ack_message))
        {
          std::cerr << "Failed to receive ACK\n";
          tcp_client.disconnect();
          camera.release();
          return 1;
        }

        if (!validateAck(ack_message, message_id))
        {
          std::cerr << "Failed to validate ACK\n";
          tcp_client.disconnect();
          camera.release();
          return 1;
        }

        std::cout << "ACK OK: " << message_id << '\n';

      }
    }
  }

  tcp_client.disconnect();
  camera.release();

  std::cout << "Edge Vision loop stopped\n";

  return 0;
}