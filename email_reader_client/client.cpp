#include "client.h"

#pragma comment(lib, "Ws2_32.lib")

void Client::Disconnect()
{
  if (this->connectSocket != INVALID_SOCKET)
  {
    shutdown(this->connectSocket, SD_SEND);
    closesocket(this->connectSocket);
    this->connectSocket = INVALID_SOCKET;
    WSACleanup();
    std::cout << "Disconnected from server." << "\n";
    isConnected = false;
  }
}

Client::~Client()
{
  Disconnect();
}

bool Client::IsConnected() const
{
  return this->isConnected;
}

void Client::ConnectToServer()
{
  WSADATA wsaDataCli;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaDataCli);
  if (iResult != 0)
  {
    std::cerr << "Unable to initialize Windows Socket" << iResult << "\n";
    return;
  }

  struct addrinfo *addrResult = NULL, *ptr = NULL, hintsCli;
  ZeroMemory(&hintsCli, sizeof(hintsCli));
  hintsCli.ai_family = AF_INET;       // IPv4
  hintsCli.ai_socktype = SOCK_STREAM; // Stream socket (TCP)
  hintsCli.ai_protocol = IPPROTO_TCP; // TCP protocol

  iResult = getaddrinfo(this->serverIP.c_str(), this->serverPort.c_str(),
                        &hintsCli, &addrResult);
  if (iResult != 0)
  {
    std::cerr << "Fail to execute getaddrinfo: " << iResult << "\n";
    WSACleanup();
    return;
  }

  for (ptr = addrResult; ptr != NULL; ptr = ptr->ai_next)
  {
    this->connectSocket =
        socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (this->connectSocket == INVALID_SOCKET)
    {
      std::cerr << "Fail to initialize TCP socket: " << WSAGetLastError()
                << "\n";
      WSACleanup();
      return;
    }

    iResult = connect(this->connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
      closesocket(this->connectSocket);
      this->connectSocket = INVALID_SOCKET;
      continue;
    }
    break;
  }

  freeaddrinfo(addrResult);

  if (this->connectSocket == INVALID_SOCKET)
  {
    std::cerr << "Cannot connect to server." << "\n";
    WSACleanup();
    return;
  }
  std::cout << "Connected to server at " << this->serverIP << ":"
            << this->serverPort << "\n";
  this->isConnected = true;
}

std::pair<std::string, std::vector<char>> Client::SendRequestAndReceiveResponse(
    const Request &request)
{
  if (this->connectSocket == INVALID_SOCKET)
  {
    return {"Client is not connected to server.", {}};
  }

  // Serialize and send the request
  CommandType commandType = request.GetCommandType();
  send(this->connectSocket, (char *)&commandType, sizeof(commandType), 0);

  const std::vector<std::string> &args = request.GetCommandArgs();
  int numArgs = args.size();
  send(this->connectSocket, (char *)&numArgs, sizeof(numArgs), 0);

  for (const auto &arg : args)
  {
    int argSize = arg.length();
    send(this->connectSocket, (char *)&argSize, sizeof(argSize), 0);
    send(this->connectSocket, arg.c_str(), argSize, 0);
  }

  // Receive the response
  // Receive string response
  int stringResponseSize;
  recv(this->connectSocket, (char *)&stringResponseSize,
       sizeof(stringResponseSize), 0);
  std::string stringResponse;
  if (stringResponseSize > 0)
  {
    std::vector<char> responseBuffer(stringResponseSize);
    recv(this->connectSocket, responseBuffer.data(), stringResponseSize, 0);
    stringResponse.assign(responseBuffer.begin(), responseBuffer.end());
  }

  // Receive file buffer response
  long long fileBufferSize;
  recv(this->connectSocket, (char *)&fileBufferSize, sizeof(fileBufferSize), 0);
  std::vector<char> fileBuffer;
  if (fileBufferSize > 0)
  {
    fileBuffer.resize(fileBufferSize);
    long long totalBytesReceived = 0;
    int iResult;
    const int CHUNK_SIZE = 8192;
    while (totalBytesReceived < fileBufferSize)
    {
      long long remainingBytes = fileBufferSize - totalBytesReceived;
      int bytesToReceive =
          (remainingBytes < CHUNK_SIZE) ? (int)remainingBytes : CHUNK_SIZE;
      iResult = recv(this->connectSocket,
                     fileBuffer.data() + totalBytesReceived, bytesToReceive, 0);
      if (iResult > 0)
      {
        totalBytesReceived += iResult;
        float progress = (float)totalBytesReceived / fileBufferSize;
        int barWidth = 50;
        std::cout << "Receiving [";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i)
        {
          if (i < pos)
            std::cout << "=";
          else if (i == pos)
            std::cout << ">";
          else
            std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << " %\r";
        std::cout.flush();
      }
      else
      {
        break;
      }
    }
    std::cout << "\n";
  }

  return {stringResponse, fileBuffer};
}

std::string Client::ListApps()
{
  Request request(LIST_APPS, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StartApp(const std::string &appName)
{
  Request request(START_APP, {appName});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StopApp(const std::string &appIdentifier)
{
  Request request(STOP_APP, {appIdentifier});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::ListProcesses()
{
  Request request(LIST_PROCESSES, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StartProcess(const std::string &processName)
{
  Request request(START_PROCESS, {processName});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StopProcess(const std::string &processIdentifier)
{
  Request request(STOP_PROCESS, {processIdentifier});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StartKeylogger()
{
  Request request(START_KEYLOGGER, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StopKeylogger()
{
  Request request(STOP_KEYLOGGER, {});
  auto response = SendRequestAndReceiveResponse(request);
  if (!response.second.empty())
  {
    std::ofstream outFile("../../files/receive/" + response.first, std::ios::binary);
    outFile.write(response.second.data(), response.second.size());
    std::cout << "File " << response.first << " received." << "\n";
    return "./files/receive/" + response.first;
  }
  return response.first;
}

std::string Client::ResetComputer()
{
  Request request(RESET_COMPUTER, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::ShutdownComputer()
{
  Request request(SHUTDOWN_COMPUTER, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::RetrieveFile(const std::string &filePath)
{
  Request request(RETRIEVE_FILE, {filePath});
  auto response = SendRequestAndReceiveResponse(request);
  if (!response.second.empty())
  {
    std::ofstream outFile("../../files/receive/" + response.first, std::ios::binary);
    //std::ofstream outFile(response.first, std::ios::binary);
    outFile.write(response.second.data(), response.second.size());
    std::cout << "File " << response.first << " received." << "\n"; 
    return "./files/receive/" + response.first;
  }
  return response.first;
}

std::string Client::StartWebcam()
{
  Request request(START_WEBCAM, {});
  auto response = SendRequestAndReceiveResponse(request);
  return response.first;
}

std::string Client::StopWebcam()
{
  Request request(STOP_WEBCAM, {});
  auto response = SendRequestAndReceiveResponse(request);
  if (!response.second.empty())
  {
    std::ofstream outFile("../../files/receive/" + response.first, std::ios::binary);
    outFile.write(response.second.data(), response.second.size());
    std::cout << "File " << response.first << " received." << "\n";
    return "./files/receive/" + response.first;
  }
  return response.first;
}

std::string Client::CaptureScreen() { 
  Request request(CAPTURE_SCREEN, {});
  auto response = SendRequestAndReceiveResponse(request);
  if (!response.second.empty())
  {
    std::ofstream outFile("../../files/receive/" + response.first, std::ios::binary);
    outFile.write(response.second.data(), response.second.size());
    std::cout << "File " << response.first << " received." << "\n";
    return "./files/receive/" + response.first;
  }
  return response.first;
}

