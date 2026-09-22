#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <queue>

#include "outbound_message.hpp"

class MessageQueue
{
public:
  bool push(OutboundMessage message);
  bool pop(OutboundMessage& message);
  void close();

private:
  std::queue<OutboundMessage> queue_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool closed_ = false;
};

#endif // MESSAGE_QUEUE_HPP