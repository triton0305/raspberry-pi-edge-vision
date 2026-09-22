#ifndef METRICS_HPP
#define METRICS_HPP

#include <chrono>
#include <cstdint>

class Metrics
{
public:
  Metrics();

  void recordFrame(double inference_ms);
  void recordMessageDelivery(double delivery_ms);

private:
  std::chrono::steady_clock::time_point last_report_time_;
  std::uint64_t frame_count_;
  std::uint64_t message_count_;
  double inference_total_ms_;
  double delivery_total_ms_;
};

#endif // METRICS_HPP