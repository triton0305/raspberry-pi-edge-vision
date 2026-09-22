#ifndef ACK_HPP
#define ACK_HPP

#include <string>

bool validateAck(
  const std::string& ack_message,
  const std::string& expected_message_id);

#endif // ACK_HPP