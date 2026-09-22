#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <cstddef>
#include <string>

class TcpClient
{
public:
  TcpClient(const std::string& server_ip, int server_port);
  ~TcpClient();

  bool connectToServer();
  bool sendData(const std::string& data);
  bool receiveData(std::string& data);
  void disconnect();
  bool isConnected() const;

private:
  bool sendAll(const void* data, std::size_t size);
  bool readAll(void* data, std::size_t size);

  std::string server_ip_;
  int server_port_;
  int socket_fd_;
  bool connected_;
};

#endif // TCP_CLIENT_HPP