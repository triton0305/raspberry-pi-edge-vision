#include "protocol/serializer.hpp"

#include <nlohmann/json.hpp>

#include "core/config.hpp"

std::string Serializer::serialize(
  const DetectionResult& result,
  const Detection& detection,
  const std::string& message_id) const
{
  nlohmann::json json;

  json["version"] = Config::PROTOCOL_VERSION;
  json["type"] = "vision";
  json["device_id"] = Config::DEVICE_ID;
  json["message_id"] = message_id;

  json["data"]["frame_id"] = result.frame_id;
  json["data"]["timestamp_ms"] = result.timestamp_ms;
  json["data"]["class_id"] = detection.class_id;
  json["data"]["class_name"] = detection.class_name;
  json["data"]["confidence"] = detection.confidence;

  json["data"]["bbox"]["x"] = detection.bbox.x;
  json["data"]["bbox"]["y"] = detection.bbox.y;
  json["data"]["bbox"]["width"] = detection.bbox.width;
  json["data"]["bbox"]["height"] = detection.bbox.height;

  return json.dump();
}