#pragma once
#include <iostream>
#include <string>
#include <vector>

enum CommandType {
  LIST_APPS,
  START_APP,
  STOP_APP,
  LIST_PROCESSES,
  START_PROCESS,
  STOP_PROCESS,
  START_KEYLOGGER,
  STOP_KEYLOGGER,
  RESET_COMPUTER,
  SHUTDOWN_COMPUTER,
  RETRIEVE_FILE,
  START_WEBCAM,
  STOP_WEBCAM,
  CAPTURE_SCREEN
};

class Request {
 private:
  CommandType commandType;
  std::vector<std::string> commandArgs;

 public:
  Request(const enum CommandType& type, const std::vector<std::string>& args)
      : commandType(type), commandArgs(args){};

  CommandType GetCommandType() const { return this->commandType; }
  const std::vector<std::string>& GetCommandArgs() const {
    return this->commandArgs;
  }
};