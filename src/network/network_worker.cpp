#include "network/network_worker.hpp"

#include <chrono>
#include <iostream>
#include <string>

#include "network/ack.hpp"
#include "core/config.hpp"

NetworkWorker::NetworkWorker(
  MessageQueue& queue,
  TcpClient& tcp_client,
  Metrics& metrics)
  : queue_(queue), tcp_client_(tcp_client), metrics_(metrics)
{
}

void NetworkWorker::run()
{
  OutboundMessage message;

  while (queue_.pop(message))
  {
    const auto delivery_start = std::chrono::steady_clock::now();

    bool ack_received = false;

    for (int attempt = 0; attempt <= Config::MAX_RETRY_COUNT; ++attempt)
    {
      if (attempt > 0)
      {
        std::cerr << "Retry " << attempt << '/'
                  << Config::MAX_RETRY_COUNT << ": "
                  << message.message_id << '\n';
      }

      if (!tcp_client_.sendData(message.payload))
      {
        std::cerr << "Failed to send detection result\n";
        fail();
        return;
      }

      std::string ack_message;

      if (!tcp_client_.receiveData(ack_message))
      {
        std::cerr << "ACK receive failed: " << message.message_id << '\n';

        tcp_client_.disconnect();

        if (attempt < Config::MAX_RETRY_COUNT)
        {
          if (!tcp_client_.connectToServer())
          {
            std::cerr << "Failed to reconnect to server\n";
            fail();
            return;
          }
        }

        continue;
      }

      std::string error_code;
      AckResult ack_result =
        checkAck(ack_message, message.message_id, error_code);

      if (ack_result == AckResult::ServerError)
      {
        std::cerr << "Server error: " << error_code << '\n';
        fail();
        return;
      }

      if (ack_result == AckResult::Invalid)
      {
        std::cerr << "Failed to validate ACK\n";
        fail();
        return;
      }

      const auto delivery_end = std::chrono::steady_clock::now();

      const double delivery_ms =
        std::chrono::duration<double, std::milli>(
          delivery_end - delivery_start).count();

      metrics_.recordMessageDelivery(delivery_ms);

      std::cout << "ACK OK: " << message.message_id << '\n';

      ack_received = true;
      break;
    }

    if (!ack_received)
    {
      std::cerr << "ACK retry limit exceeded: "
                << message.message_id << '\n';

      fail();
      return;
    }
  }
}

bool NetworkWorker::hasFailed() const
{
  return failed_.load();
}

void NetworkWorker::fail()
{
  failed_.store(true);
  tcp_client_.disconnect();
  queue_.close();
}