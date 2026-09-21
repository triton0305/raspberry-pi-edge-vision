#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <string>

class TcpClient
{
public:
  TcpClient(const std::string& server_ip, int server_port);
  ~TcpClient();

  bool connectToServer();
  bool sendData(const std::string& data);
  void disconnect();

  bool isConnected() const;

private:
  std::string server_ip_;
  int server_port_;
  int socket_fd_;
  bool connected_;
};

#endif