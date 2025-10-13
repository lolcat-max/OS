#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

DWORD get_pid_by_name(const std::wstring& name) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe32{ sizeof(pe32) };
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (name == pe32.szExeFile) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

// Simple approach: Just keep suspending from external process
int main(int argc, char* argv[]) {
    std::wcout << L"=== Simple Continuous Suspend ===" << std::endl;

    if (argc < 2) {
        std::wcout << L"Usage: injector.exe <process_name>" << std::endl;
        std::wcout << L"Example: injector.exe notepad.exe" << std::endl;
        return 1;
    }

    std::string arg_narrow(argv[1]);
    std::wstring target(arg_narrow.begin(), arg_narrow.end());

    DWORD pid = get_pid_by_name(target);
    if (!pid) {
        std::wcerr << L"Process not found!" << std::endl;
        return 1;
    }

    std::wcout << L"Found PID: " << pid << std::endl;
    std::wcout << L"Starting continuous suspend loop..." << std::endl;

    // Open process once
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::wcerr << L"Failed to open process. Run as Administrator!" << std::endl;
        return 1;
    }

    // Continuous suspend loop - runs in THIS process, not injected
    int cycle = 0;
    while (true) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);

            int suspended_count = 0;
            if (Thread32First(hSnapshot, &te)) {
                do {
                    if (te.th32OwnerProcessID == pid) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                        if (hThread) {
                            SuspendThread(hThread);
                            suspended_count++;
                            CloseHandle(hThread);
                        }
                    }
                } while (Thread32Next(hSnapshot, &te));
            }
            CloseHandle(hSnapshot);

            if (cycle % 10 == 0) { // Print every 10 cycles
                std::wcout << L"Suspended " << suspended_count << L" threads (cycle " << cycle << L")" << std::endl;
            }
        }

        // Check if process still exists
        DWORD exit_code;
        if (!GetExitCodeProcess(hProcess, &exit_code) || exit_code != STILL_ACTIVE) {
            std::wcerr << L"\nTarget process terminated!" << std::endl;
            break;
        }

        Sleep(100); // Re-suspend every 100ms
        cycle++;
    }

    CloseHandle(hProcess);
    std::wcout << L"Suspend loop ended." << std::endl;
    return 0;
}
