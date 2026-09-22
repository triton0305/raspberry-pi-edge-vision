#ifndef DETECTION_RESULT_HPP
#define DETECTION_RESULT_HPP

#include <cstdint>
#include <string>

struct BoundingBox
{
  int x;
  int y;
  int width;
  int height;
};

struct Detection
{
  int class_id;
  std::string class_name;
  float confidence;
  BoundingBox bbox;
};

struct DetectionResult
{
  uint64_t frame_id;
  int64_t timestamp_ms;
};

#endif // DETECTION_RESULT_HPP