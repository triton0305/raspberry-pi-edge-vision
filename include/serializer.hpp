#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>

#include "detection_result.hpp"

class Serializer
{
public:
  std::string serialize(const DetectionResult& result) const;
};

#endif // SERIALIZER_HPP