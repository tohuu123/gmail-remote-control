#pragma once
#define _WIN32_WINNT 0x0600
#define DEFAULT_CLI_BUFLEN 512
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "./request/request.h"

class Client {
 private:
  std::string serverIP;
  std::string serverPort;
  bool isConnected = false;
  SOCKET connectSocket = INVALID_SOCKET;

  std::pair<std::string, std::vector<char>> SendRequestAndReceiveResponse(
      const Request& request);

 public:
Client() {
    serverIP = "";
    serverPort = "";
    isConnected = false;
    connectSocket = INVALID_SOCKET;
};
  Client(const std::string& ip, const std::string& port)
      : serverIP(ip), serverPort(port) {};
  ~Client();

  void SetDestinationIP(const std::string& ip) { serverIP = ip; };
  void SetDestinationPort(const std::string& port) { serverPort = port; };
  void ConnectToServer();   
  void Disconnect();
  bool IsConnected() const;

  std::string ListApps();
  std::string StartApp(const std::string& appName);
  std::string StopApp(const std::string& appIdentifier);
  std::string ListProcesses();
  std::string StartProcess(const std::string& processName);
  std::string StopProcess(const std::string& processIdentifier);
  std::string StartKeylogger();
  std::string StopKeylogger();
  std::string ResetComputer();
  std::string ShutdownComputer();
  std::string RetrieveFile(const std::string& filePath);
  std::string StartWebcam();
  std::string StopWebcam();
  std::string CaptureScreen();
};