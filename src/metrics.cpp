#include "metrics.hpp"

#include <iostream>

Metrics::Metrics()
  : last_report_time_(std::chrono::steady_clock::now()),
    frame_count_(0),
    message_count_(0),
    inference_total_ms_(0.0),
    delivery_total_ms_(0.0)
{
}

void Metrics::recordFrame(double inference_ms, std::size_t queue_size, std::uint64_t dropped_count)
{
  std::lock_guard<std::mutex> lock(mutex_);

  ++frame_count_;
  inference_total_ms_ += inference_ms;

  const auto now = std::chrono::steady_clock::now();
  const double elapsed_seconds =
    std::chrono::duration<double>(now - last_report_time_).count();

  if (elapsed_seconds < 1.0)
    return;

  const double effective_fps = frame_count_ / elapsed_seconds;
  const double average_inference_ms = inference_total_ms_ / frame_count_;

  std::cout << "Metrics: "
          << "Effective FPS=" << effective_fps
          << " | Avg Inference=" << average_inference_ms << " ms"
          << " | Queue=" << queue_size
          << " | Dropped=" << dropped_count;

  if (message_count_ > 0)
  {
    const double average_delivery_ms = delivery_total_ms_ / message_count_;
    std::cout << " | Avg Delivery=" << average_delivery_ms << " ms";
  }

  std::cout << '\n';

  last_report_time_ = now;
  frame_count_ = 0;
  message_count_ = 0;
  inference_total_ms_ = 0.0;
  delivery_total_ms_ = 0.0;
}

void Metrics::recordMessageDelivery(double delivery_ms)
{
  ++message_count_;
  delivery_total_ms_ += delivery_ms;
}