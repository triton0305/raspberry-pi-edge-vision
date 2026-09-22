#ifndef NETWORK_WORKER_HPP
#define NETWORK_WORKER_HPP

#include <atomic>

#include "network/message_queue.hpp"
#include "core/metrics.hpp"
#include "network/tcp_client.hpp"

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