#include "network/ack.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

#include "core/config.hpp"

AckResult checkAck(
  const std::string& ack_message,
  const std::string& expected_message_id,
  std::string& error_code)
{
  error_code.clear();

  nlohmann::json ack = nlohmann::json::parse(ack_message, nullptr, false);

  if (ack.is_discarded())
  {
    std::cerr << "Invalid ACK JSON\n";
    return AckResult::Invalid;
  }

  if (ack.value("version", 0) != Config::PROTOCOL_VERSION)
  {
    std::cerr << "Invalid ACK version\n";
    return AckResult::Invalid;
  }

  if (ack.value("type", "") != "ack")
  {
    std::cerr << "Invalid ACK type\n";
    return AckResult::Invalid;
  }

  if (ack.value("message_id", "") != expected_message_id)
  {
    std::cerr << "ACK message_id mismatch\n";
    return AckResult::Invalid;
  }

  const std::string status = ack.value("status", "");

  if (status == "ok")
  {
    return AckResult::Ok;
  }

  if (status == "error")
  {
    error_code = ack.value("error_code", "");

    if (error_code.empty())
    {
      std::cerr << "Missing ACK error_code\n";
      return AckResult::Invalid;
    }

    return AckResult::ServerError;
  }

  std::cerr << "Invalid ACK status\n";
  return AckResult::Invalid;
}
