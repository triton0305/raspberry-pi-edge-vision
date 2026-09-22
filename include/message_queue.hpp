#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>

#include "outbound_message.hpp"

class MessageQueue
{
public:
  explicit MessageQueue(std::size_t max_size);

  bool push(OutboundMessage message);
  bool pop(OutboundMessage& message);
  void close();

  std::size_t size() const;
  std::uint64_t droppedCount() const;

private:
  std::queue<OutboundMessage> queue_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::size_t max_size_;
  std::uint64_t dropped_count_ = 0;
  bool closed_ = false;
};

#endif // MESSAGE_QUEUE_HPP