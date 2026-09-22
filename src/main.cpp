#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <exception>
#include <csignal>
#include <thread>

#include "camera.hpp"
#include "config.hpp"
#include "detection_result.hpp"
#include "detector.hpp"
#include "postprocessor.hpp"
#include "preprocessor.hpp"
#include "serializer.hpp"
#include "tcp_client.hpp"
#include "ack.hpp"
#include "metrics.hpp"
#include "message_queue.hpp"
#include "network_worker.hpp"

volatile std::sig_atomic_t running = 1;

void handleSignal(int)
{
  running = 0;
}

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

int main(int argc, char* argv[])
{
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port>\n";
    return 1;
  }

  const std::string server_ip = argv[1];
  int server_port = 0;

  try
  {
    server_port = std::stoi(argv[2]);
  }
  catch (const std::exception&)
  {
    std::cerr << "Invalid server port\n";
    return 1;
  }

  if (server_port < 1 || server_port > 65535)
  {
    std::cerr << "Server port must be between 1 and 65535\n";
    return 1;
  }

  const std::string model_path = MODEL_PATH;

  Camera camera(0, 640, 480, 30);
  Preprocessor preprocessor(640, 640);
  Detector detector(model_path);
  PostProcessor postprocessor(0.25f, 0.45f);
  Serializer serializer;
  TcpClient tcp_client(server_ip, server_port);
  Metrics metrics;
  MessageQueue message_queue;
  NetworkWorker network_worker(message_queue, tcp_client, metrics);

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

  std::thread network_thread(&NetworkWorker::run, &network_worker);

  std::cout << "Edge Vision loop started\n";
  std::cout << "Boot ID: " << boot_id << '\n';
  std::cout << "Press Ctrl+C to quit\n";

  while (running)
  {
    if (network_worker.hasFailed())
    {
      std::cerr << "Network worker failed\n";
      break;
    }

    cv::Mat frame;

    if (!camera.read(frame))
    {
      std::cerr << "Failed to capture frame\n";
      break;
    }

    const std::uint64_t current_frame_id = frame_id++;
    const std::int64_t timestamp_ms = currentUnixTimeMs();

    cv::Mat blob = preprocessor.process(frame);

    const auto inference_start = std::chrono::steady_clock::now();
    std::vector<cv::Mat> outputs = detector.infer(blob);
    const auto inference_end = std::chrono::steady_clock::now();

    const double inference_ms =
      std::chrono::duration<double, std::milli>(inference_end - inference_start).count();

    std::vector<Detection> detections = postprocessor.process(
      outputs, frame.cols, frame.rows,
      preprocessor.inputWidth(), preprocessor.inputHeight());

    metrics.recordFrame(inference_ms);

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

        if (!message_queue.push({message_id, message}))
        {
          std::cerr << "Failed to enqueue message\n";
          running = 0;
          break;
        }
      }
    }
  }

  message_queue.close();

  if (network_thread.joinable())
    network_thread.join();



  tcp_client.disconnect();
  camera.release();

  std::cout << "Edge Vision loop stopped\n";

  return 0;
}