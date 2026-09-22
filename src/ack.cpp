#include "ack.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

#include "config.hpp"

bool validateAck(
  const std::string& ack_message,
  const std::string& expected_message_id)
{
  nlohmann::json ack = nlohmann::json::parse(ack_message, nullptr, false);

  if (ack.is_discarded())
  {
    std::cerr << "Invalid ACK JSON\n";
    return false;
  }

  if (ack.value("version", 0) != Config::PROTOCOL_VERSION)
  {
    std::cerr << "Invalid ACK version\n";
    return false;
  }

  if (ack.value("type", "") != "ack")
  {
    std::cerr << "Invalid ACK type\n";
    return false;
  }

  if (ack.value("message_id", "") != expected_message_id)
  {
    std::cerr << "ACK message_id mismatch\n";
    return false;
  }

  if (ack.value("status", "") != "ok")
  {
    std::cerr << "ACK status is not ok\n";
    return false;
  }

  return true;
}