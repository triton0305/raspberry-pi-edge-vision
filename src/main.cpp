#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "core/boot_id.hpp"
#include "vision/camera.hpp"
#include "core/config.hpp"
#include "vision/detector.hpp"
#include "network/message_queue.hpp"
#include "core/metrics.hpp"
#include "network/network_worker.hpp"
#include "vision/postprocessor.hpp"
#include "vision/preprocessor.hpp"
#include "protocol/serializer.hpp"
#include "network/tcp_client.hpp"
#include "vision/vision_worker.hpp"

volatile std::sig_atomic_t running = 1;

void handleSignal(int)
{
  running = 0;
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
  MessageQueue message_queue(Config::MAX_QUEUE_SIZE);
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

  VisionWorker vision_worker(
    camera, preprocessor, detector, postprocessor,
    serializer, message_queue, network_worker, metrics,
    boot_id, running);

  std::thread network_thread(&NetworkWorker::run, &network_worker);
  vision_worker.run();

  message_queue.close();

  if (network_thread.joinable())
    network_thread.join();

  tcp_client.disconnect();
  camera.release();

  std::cout << "Edge Vision loop stopped\n";

  return 0;
}