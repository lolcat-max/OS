#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <set>

std::set<std::wstring> blocked_programs;
DWORD last_focused_pid = 0;

void RunCommand(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring full_cmd = L"cmd.exe /c " + cmd + L" >nul 2>&1";

    if (CreateProcessW(NULL, (LPWSTR)full_cmd.c_str(), NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

std::wstring GetProcessName(DWORD pid) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return L"Unknown";
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(h, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                CloseHandle(h);
                return pe.szExeFile;
            }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return L"Unknown";
}

std::wstring GetProcessPath(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return path;
    }
    CloseHandle(hProcess);
    return L"";
}

bool IsSystemProcess(const std::wstring& name) {
    std::wstring lower = name;
    for (auto& c : lower) c = towlower(c);

    // EXPANDED list - need all these for internet to work
    return (lower == L"svchost.exe" ||
            lower == L"system" ||
            lower == L"services.exe" ||
            lower == L"lsass.exe" ||
            lower == L"csrss.exe" ||
            lower == L"dwm.exe" ||
            lower == L"explorer.exe" ||
            lower == L"runtimebroker.exe" ||
            lower == L"taskhostw.exe" ||
            lower == L"searchapp.exe" ||
            lower == L"sihost.exe");
}

void InitializeMinimalServices() {
    std::wcout << L"[INIT] Allowing essential Windows services..." << std::endl;

    // Critical Windows networking components
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_svchost\" dir=out action=allow program=\"C:\\Windows\\System32\\svchost.exe\" enable=yes");
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_DNS\" dir=out action=allow protocol=UDP remoteport=53 enable=yes");
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_DHCP\" dir=out action=allow protocol=UDP remoteport=67-68 enable=yes");
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_HTTP\" dir=out action=allow protocol=TCP remoteport=80 enable=yes");
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_HTTPS\" dir=out action=allow protocol=TCP remoteport=443 enable=yes");
    RunCommand(L"netsh advfirewall firewall add rule name=\"Min_Explorer\" dir=out action=allow program=\"C:\\Windows\\explorer.exe\" enable=yes");

    Sleep(500);
    std::wcout << L"[INIT] Services configured." << std::endl;
}

void BlockAllExcept(DWORD allowed_pid) {
    // Clear previous blocks
    for (const auto& prog : blocked_programs) {
        RunCommand(L"netsh advfirewall firewall delete rule name=\"Block_" + prog + L"\"");
    }
    blocked_programs.clear();

    // Get all processes
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(h, &pe)) {
        do {
            DWORD pid = pe.th32ProcessID;
            if (pid <= 4) continue;

            std::wstring name = pe.szExeFile;

            // Skip system processes and current process
            if (IsSystemProcess(name) || pid == GetCurrentProcessId()) continue;

            // Block everything except focused
            if (pid != allowed_pid) {
                std::wstring path = GetProcessPath(pid);
                if (!path.empty()) {
                    std::wstring rule = L"Block_" + name + L"_" + std::to_wstring(pid);
                    std::wstring cmd = L"netsh advfirewall firewall add rule name=\"" + rule +
                                      L"\" dir=out action=block program=\"" + path + L"\" enable=yes";

                    RunCommand(cmd);
                    blocked_programs.insert(name + L"_" + std::to_wstring(pid));
                }
            }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
}

void CleanupAll() {
    std::wcout << L"\n[CLEANUP] Restoring normal firewall..." << std::endl;

    for (const auto& prog : blocked_programs) {
        RunCommand(L"netsh advfirewall firewall delete rule name=\"Block_" + prog + L"\"");
    }

    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_svchost\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_DNS\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_DHCP\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_HTTP\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_HTTPS\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Min_Explorer\"");
    RunCommand(L"netsh advfirewall firewall delete rule name=\"Block_*\"");

    Sleep(500);
    std::wcout << L"[CLEANUP] Internet restored." << std::endl;
}

int main() {
    std::wcout << L"Focus Firewall - RUN AS ADMIN" << std::endl;
    std::wcout << L"Press ESC to exit\n" << std::endl;

    InitializeMinimalServices();
    std::wcout << L"\nMonitoring focus...\n" << std::endl;

    bool running = true;

    while (running) {
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);

            if (pid != last_focused_pid && pid != 0) {
                std::wstring name = GetProcessName(pid);

                if (!IsSystemProcess(name)) {
                    std::wcout << L"[FOCUS] " << name << L" (" << pid << L")" << std::endl;
                    BlockAllExcept(pid);
                }

                last_focused_pid = pid;
            }
        }

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            running = false;
        }

        Sleep(1000);
    }

    CleanupAll();
    return 0;
}
