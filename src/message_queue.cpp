#include "message_queue.hpp"

#include <utility>

bool MessageQueue::push(OutboundMessage message)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (closed_)
      return false;

    queue_.push(std::move(message));
  }

  condition_.notify_one();

  return true;
}

bool MessageQueue::pop(OutboundMessage& message)
{
  std::unique_lock<std::mutex> lock(mutex_);

  condition_.wait(lock, [this]
  {
    return closed_ || !queue_.empty();
  });

  if (queue_.empty())
    return false;

  message = std::move(queue_.front());
  queue_.pop();

  return true;
}

void MessageQueue::close()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }

  condition_.notify_all();
}