#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <set>

std::set<DWORD> blocked_pids;
DWORD last_focused_pid = 0;

// Get process name from PID
std::wstring GetProcessName(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return L"Unknown";

    PROCESSENTRY32W pe32{ sizeof(pe32) };
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                CloseHandle(hSnapshot);
                return std::wstring(pe32.szExeFile);
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return L"Unknown";
}

// Get full executable path from PID
std::wstring GetProcessPath(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return std::wstring(path);
    }
    CloseHandle(hProcess);
    return L"";
}

// Check if rule exists
bool RuleExists(const std::wstring& rule_name) {
    std::wstring cmd = L"netsh advfirewall firewall show rule name=\"" + rule_name + L"\" >nul 2>&1";
    return _wsystem(cmd.c_str()) == 0;
}

// Block a process
void BlockProcess(DWORD pid, const std::wstring& name) {
    std::wstring path = GetProcessPath(pid);
    if (path.empty()) return;

    std::wstring rule_name = L"FocusFW_" + std::to_wstring(pid);

    // Only add rule if it doesn't exist
    if (!RuleExists(rule_name)) {
        std::wstring cmd = L"netsh advfirewall firewall add rule name=\"" + rule_name +
                          L"\" dir=out action=block program=\"" + path +
                          L"\" enable=yes >nul 2>&1";
        _wsystem(cmd.c_str());
        std::wcout << L"[BLOCKED] " << name << L" (PID: " << pid << L")" << std::endl;
        blocked_pids.insert(pid);
    }
}

// Unblock a process
void UnblockProcess(DWORD pid, const std::wstring& name) {
    std::wstring rule_name = L"FocusFW_" + std::to_wstring(pid);

    // Only delete if rule exists
    if (RuleExists(rule_name)) {
        std::wstring cmd = L"netsh advfirewall firewall delete rule name=\"" + rule_name + L"\" >nul 2>&1";
        _wsystem(cmd.c_str());
        std::wcout << L"[UNBLOCKED] " << name << L" (PID: " << pid << L")" << std::endl;
    }
    blocked_pids.erase(pid);
}

// Block all processes except the focused one
void UpdateFirewall(DWORD focused_pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32{ sizeof(pe32) };
    std::set<DWORD> current_pids;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            DWORD pid = pe32.th32ProcessID;
            if (pid <= 4) continue; // Skip system idle/system

            current_pids.insert(pid);
            std::wstring name = pe32.szExeFile;

            // Skip critical system processes
            if (name == L"System" || name == L"svchost.exe" ||
                name == L"explorer.exe" || name == L"dwm.exe" ||
                name == L"csrss.exe" || name == L"services.exe" ||
                name == L"lsass.exe" || name == L"winlogon.exe") {
                continue;
            }

            // Skip our own process
            if (pid == GetCurrentProcessId()) continue;

            // Block if not focused
            if (pid != focused_pid) {
                BlockProcess(pid, name);
            } else {
                UnblockProcess(pid, name);
            }

        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    // Clean up rules for terminated processes
    std::set<DWORD> to_remove;
    for (DWORD pid : blocked_pids) {
        if (current_pids.find(pid) == current_pids.end()) {
            to_remove.insert(pid);
        }
    }

    for (DWORD pid : to_remove) {
        std::wstring rule_name = L"FocusFW_" + std::to_wstring(pid);
        _wsystem((L"netsh advfirewall firewall delete rule name=\"" + rule_name + L"\" >nul 2>&1").c_str());
        blocked_pids.erase(pid);
    }
}

// Clean up all our firewall rules
void CleanupAllRules() {
    std::wcout << L"\nCleaning up firewall rules..." << std::endl;

    for (DWORD pid : blocked_pids) {
        std::wstring rule_name = L"FocusFW_" + std::to_wstring(pid);
        _wsystem((L"netsh advfirewall firewall delete rule name=\"" + rule_name + L"\" >nul 2>&1").c_str());
    }

    // Wildcard cleanup for any remaining rules
    _wsystem(L"netsh advfirewall firewall delete rule name=\"FocusFW_*\" >nul 2>&1");

    blocked_pids.clear();
    std::wcout << L"Cleanup complete." << std::endl;
}

int main() {
    std::wcout << L"=== Focus-Based Firewall ===" << std::endl;
    std::wcout << L"*** MUST RUN AS ADMINISTRATOR ***\n" << std::endl;
    std::wcout << L"Only the focused window gets network access." << std::endl;
    std::wcout << L"Press ESC to exit.\n" << std::endl;

    Sleep(1000);

    bool running = true;
    int update_counter = 0;

    while (running) {
        // Get focused window
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            DWORD focused_pid = 0;
            GetWindowThreadProcessId(hwnd, &focused_pid);

            if (focused_pid != last_focused_pid && focused_pid != 0) {
                std::wstring proc_name = GetProcessName(focused_pid);
                std::wcout << L"\n[FOCUS] " << proc_name << L" (PID: " << focused_pid << L")" << std::endl;

                // Update firewall rules
                UpdateFirewall(focused_pid);

                last_focused_pid = focused_pid;
                update_counter = 0;
            }
        }

        // Periodic refresh every 10 seconds
        if (update_counter++ > 20) {
            if (last_focused_pid != 0) {
                UpdateFirewall(last_focused_pid);
            }
            update_counter = 0;
        }

        // Check for ESC
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            std::wcout << L"\nExiting..." << std::endl;
            running = false;
        }

        Sleep(5);
    }

    CleanupAllRules();
    std::wcout << L"Network access restored for all processes." << std::endl;

    return 0;
}
