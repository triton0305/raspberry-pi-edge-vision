#include "serializer.hpp"

#include <sstream>

std::string Serializer::serialize(const DetectionResult& result) const
{
  std::ostringstream oss;

  for(const Detection& detection : result.detections)
  {
    oss << result.frame_id << '|'
        << result.timestamp_ms << '|'
        << detection.class_id << '|'
        << detection.class_name << '|'
        << detection.confidence << '|'
        << detection.bbox.x << '|'
        << detection.bbox.y << '|'
        << detection.bbox.width << '|'
        << detection.bbox.height << '\n';
  }

  return oss.str();
}