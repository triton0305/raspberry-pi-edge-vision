#ifndef ACK_HPP
#define ACK_HPP

#include <string>

enum class AckResult
{
  Ok,
  ServerError,
  Invalid
};

AckResult checkAck(
  const std::string& ack_message,
  const std::string& expected_message_id,
  std::string& error_code);

bool validateAck(
  const std::string& ack_message,
  const std::string& expected_message_id);

#endif // ACK_HPP