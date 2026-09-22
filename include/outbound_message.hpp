#ifndef OUTBOUND_MESSAGE_HPP
#define OUTBOUND_MESSAGE_HPP

#include <string>

struct OutboundMessage
{
  std::string message_id;
  std::string payload;
};

#endif // OUTBOUND_MESSAGE_HPPs