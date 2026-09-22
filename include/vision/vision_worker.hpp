#ifndef VISION_WORKER_HPP
#define VISION_WORKER_HPP

#include <csignal>
#include <cstdint>

class Camera;
class Preprocessor;
class Detector;
class PostProcessor;
class Serializer;
class MessageQueue;
class NetworkWorker;
class Metrics;

class VisionWorker
{
public:
  VisionWorker(
    Camera& camera,
    Preprocessor& preprocessor,
    Detector& detector,
    PostProcessor& postprocessor,
    Serializer& serializer,
    MessageQueue& message_queue,
    NetworkWorker& network_worker,
    Metrics& metrics,
    std::uint64_t boot_id,
    volatile std::sig_atomic_t& running);

  void run();

private:
  Camera& camera_;
  Preprocessor& preprocessor_;
  Detector& detector_;
  PostProcessor& postprocessor_;
  Serializer& serializer_;
  MessageQueue& message_queue_;
  NetworkWorker& network_worker_;
  Metrics& metrics_;
  std::uint64_t boot_id_;
  volatile std::sig_atomic_t& running_;
};

#endif // VISION_WORKER_HPP
