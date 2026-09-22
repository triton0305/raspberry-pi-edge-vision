#include "vision/vision_worker.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "core/detection_result.hpp"
#include "core/message_id.hpp"
#include "vision/camera.hpp"
#include "vision/preprocessor.hpp"
#include "vision/detector.hpp"
#include "vision/postprocessor.hpp"
#include "protocol/serializer.hpp"
#include "network/message_queue.hpp"
#include "network/network_worker.hpp"
#include "core/metrics.hpp"

namespace
{
std::int64_t currentUnixTimeMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
}

VisionWorker::VisionWorker(
  Camera& camera,
  Preprocessor& preprocessor,
  Detector& detector,
  PostProcessor& postprocessor,
  Serializer& serializer,
  MessageQueue& message_queue,
  NetworkWorker& network_worker,
  Metrics& metrics,
  std::uint64_t boot_id,
  volatile std::sig_atomic_t& running)
  : camera_(camera),
    preprocessor_(preprocessor),
    detector_(detector),
    postprocessor_(postprocessor),
    serializer_(serializer),
    message_queue_(message_queue),
    network_worker_(network_worker),
    metrics_(metrics),
    boot_id_(boot_id),
    running_(running)
{
}

void VisionWorker::run()
{
  std::uint64_t frame_id = 0;
  std::uint64_t sequence = 0;

  std::cout << "Edge Vision loop started\n";
  std::cout << "Boot ID: " << boot_id_ << '\n';
  std::cout << "Press Ctrl+C to quit\n";

  while (running_)
  {
    if (network_worker_.hasFailed())
    {
      std::cerr << "Network worker failed\n";
      break;
    }

    cv::Mat frame;

    if (!camera_.read(frame))
    {
      std::cerr << "Failed to capture frame\n";
      break;
    }

    const std::uint64_t current_frame_id = frame_id++;
    const std::int64_t timestamp_ms = currentUnixTimeMs();

    cv::Mat blob = preprocessor_.process(frame);

    const auto inference_start = std::chrono::steady_clock::now();
    std::vector<cv::Mat> outputs = detector_.infer(blob);
    const auto inference_end = std::chrono::steady_clock::now();

    const double inference_ms =
      std::chrono::duration<double, std::milli>(inference_end - inference_start).count();

    std::vector<Detection> detections = postprocessor_.process(
      outputs, frame.cols, frame.rows,
      preprocessor_.inputWidth(), preprocessor_.inputHeight());

    for (const Detection& detection : detections)
    {
      ++sequence;

      const std::string message_id = createMessageId(boot_id_, sequence);

      DetectionResult result;
      result.frame_id = current_frame_id;
      result.timestamp_ms = timestamp_ms;

      std::string message = serializer_.serialize(result, detection, message_id);

      if (!message.empty())
      {
        std::cout << message << '\n';

        if (!message_queue_.push({message_id, message}))
        {
          std::cerr << "Failed to enqueue message\n";
          running_ = 0;
          break;
        }
      }
    }

    metrics_.recordFrame(
      inference_ms,
      message_queue_.size(),
      message_queue_.droppedCount());
  }
}
