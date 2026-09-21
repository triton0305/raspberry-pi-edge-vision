#include "tcp_client.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

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
  if (connected_)
  {
    return true;
  }

  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0)
  {
    std::cerr << "TcpClient: socket creation failed\n";
    return false;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port_);

  if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) != 1)
  {
    std::cerr << "TcpClient: invalid server IP\n";
    close(socket_fd_);
    socket_fd_ = -1;
    return false;
  }

  if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
  {
    std::cerr << "TcpClient: connection failed\n";
    close(socket_fd_);
    socket_fd_ = -1;
    return false;
  }

  connected_ = true;
  std::cout << "TcpClient: connected to " << server_ip_ << ":" << server_port_ << '\n';

  return true;
}

bool TcpClient::sendData(const std::string& data)
{
  if (!connected_)
  {
    std::cerr << "TcpClient: not connected\n";
    return false;
  }

  size_t total_sent = 0;

  while (total_sent < data.size())
  {
    ssize_t sent = send(socket_fd_, data.data() + total_sent, data.size() - total_sent, 0);

    if (sent <= 0)
    {
      std::cerr << "TcpClient: send failed\n";
      disconnect();
      return false;
    }

    total_sent += static_cast<size_t>(sent);
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