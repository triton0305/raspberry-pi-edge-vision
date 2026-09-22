#include "network/tcp_client.hpp"
#include "core/config.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <iostream>

TcpClient::TcpClient(const std::string& server_ip, int server_port)
  : server_ip_(server_ip),
    server_port_(server_port),
    socket_fd_(-1),
    connected_(false)
{
}

TcpClient::~TcpClient()
{
  disconnect();
}

bool TcpClient::connectToServer()
{
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);

  if (socket_fd_ < 0)
  {
    std::cerr << "Failed to create socket\n";
    return false;
  }

  timeval receive_timeout{};
  receive_timeout.tv_sec = Config::ACK_TIMEOUT_MS / 1000;
  receive_timeout.tv_usec = (Config::ACK_TIMEOUT_MS % 1000) * 1000;

  if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout)) < 0)
  {
    std::cerr << "Failed to set receive timeout\n";
    disconnect();
    return false;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port_);

  if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0)
  {
    std::cerr << "Invalid server address\n";
    disconnect();
    return false;
  }

  if (connect(
        socket_fd_,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)) < 0)
  {
    std::cerr << "Failed to connect to server\n";
    disconnect();
    return false;
  }

  connected_ = true;

  return true;
}

bool TcpClient::sendAll(const void* data, std::size_t size)
{
  const char* buffer = static_cast<const char*>(data);
  std::size_t total_sent = 0;

  while (total_sent < size)
  {
    ssize_t sent = send(socket_fd_, buffer + total_sent, size - total_sent, MSG_NOSIGNAL);

    if (sent < 0)
    {
      if (errno == EINTR)
        continue;

      std::cerr << "Failed to send data\n";
      disconnect();
      return false;
    }

    if (sent == 0)
    {
      std::cerr << "Connection closed while sending data\n";
      disconnect();
      return false;
    }

    total_sent += static_cast<std::size_t>(sent);
  }

  return true;
}

bool TcpClient::readAll(void* data, std::size_t size)
{
  char* buffer = static_cast<char*>(data);
  std::size_t total_received = 0;

  while (total_received < size)
  {
    ssize_t received = recv(
      socket_fd_,
      buffer + total_received,
      size - total_received,
      0);

    if (received == 0)
    {
      std::cerr << "Server disconnected\n";
      disconnect();
      return false;
    }

    if (received < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        std::cerr << "Receive timeout: received " << total_received
                  << '/' << size << " bytes in current read\n";
        return false;
      }

      std::cerr << "Failed to receive data\n";
      disconnect();
      return false;
    }

    total_received += static_cast<std::size_t>(received);
  }

  return true;
}

bool TcpClient::sendData(const std::string& data)
{
  if (!connected_)
  {
    std::cerr << "TCP client is not connected\n";
    return false;
  }

  const std::uint32_t payload_size = static_cast<std::uint32_t>(data.size());
  const std::uint32_t net_size = htonl(payload_size);

  if (!sendAll(&net_size, sizeof(net_size)))
  {
    return false;
  }

  if (!sendAll(data.data(), data.size()))
  {
    return false;
  }

  return true;
}

bool TcpClient::receiveData(std::string& data)
{
  if (!connected_)
  {
    std::cerr << "TCP client is not connected\n";
    return false;
  }

  std::uint32_t net_size = 0;

  if (!readAll(&net_size, sizeof(net_size)))
  {
    std::cerr << "Failed to receive ACK length prefix (4-byte big-endian)\n";
    return false;
  }

  const std::uint32_t payload_size = ntohl(net_size);

  if (payload_size == 0 || payload_size > 1024 * 1024)
  {
    std::cerr << "Invalid payload size: " << payload_size << '\n';
    disconnect();
    return false;
  }

  data.resize(payload_size);

  if (!readAll(data.data(), data.size()))
  {
    std::cerr << "Failed to receive ACK payload: expected "
              << payload_size << " bytes\n";
    return false;
  }

  return true;
}

void TcpClient::disconnect()
{
  if (socket_fd_ >= 0)
  {
    close(socket_fd_);
    socket_fd_ = -1;
  }

  connected_ = false;
}

bool TcpClient::isConnected() const
{
  return connected_;
}
