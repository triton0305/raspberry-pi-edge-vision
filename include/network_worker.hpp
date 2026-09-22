#ifndef NETWORK_WORKER_HPP
#define NETWORK_WORKER_HPP

#include <atomic>

#include "message_queue.hpp"
#include "metrics.hpp"
#include "tcp_client.hpp"

class NetworkWorker
{
public:
  NetworkWorker(MessageQueue& queue, TcpClient& tcp_client, Metrics& metrics);

  void run();
  bool hasFailed() const;

private:
  void fail();

  MessageQueue& queue_;
  TcpClient& tcp_client_;
  Metrics& metrics_;
  std::atomic<bool> failed_{false};
};

#endif // NETWORK_WORKER_HPP