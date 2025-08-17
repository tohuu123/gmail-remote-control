#pragma once
#define _WIN32_WINNT 0x0600
#define DEFAULT_CLI_BUFLEN 512
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <vector>
#include "./src/webcam.cpp"
#include "../email_reader_client/request/request.h"

class Server {
 private:
  std::string serverPort;
  SOCKET listenSocket = INVALID_SOCKET;
  SOCKET clientCommunicationSocket = INVALID_SOCKET;
  bool isRunning = false;
  void HandleClient();

 public:
  Server(const std::string& port);
  ~Server();
  void Start();
};
