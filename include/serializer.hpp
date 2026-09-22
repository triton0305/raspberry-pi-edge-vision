#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>

#include "detection_result.hpp"

class Serializer
{
public:
  std::string serialize(
    const DetectionResult& result,
    const Detection& detection,
    const std::string& message_id) const;
};

#endif // SERIALIZER_HPP