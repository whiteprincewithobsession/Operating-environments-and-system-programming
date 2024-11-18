#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>
#include <TlHelp32.h>

std::vector<PROCESS_INFORMATION> childProcesses;
bool keepHandles = true;

void StartChildProcess() {
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    std::wstring commandLine = L"notepad.exe";

    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, &commandLine[0], NULL, NULL, keepHandles, 0, NULL, NULL, &si, &pi)) {
        childProcesses.push_back(pi);
        std::cout << "Child process started with PID: " << pi.dwProcessId << "\n\n";
    }
    else {
        DWORD errorCode = GetLastError();
        std::cout << "Child Process Start Error: " << errorCode << " error code\n\n";
    }
}

void TerminateChildProcesses() {
    std::vector<PROCESS_INFORMATION> terminatedProcesses;

    for (const auto& pi : childProcesses) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pi.dwProcessId);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            std::cout << "Child process with PID " << pi.dwProcessId << " terminated\n\n";
            if (!keepHandles) {
                terminatedProcesses.push_back(pi);
            }
        }
        else {
            std::cout << "Failed to terminate child process with PID " << pi.dwProcessId << "\n\n";
        }
    }

    if (!keepHandles) {
        for (const auto& pi : terminatedProcesses) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        childProcesses.erase(std::remove_if(childProcesses.begin(), childProcesses.end(), [&](const PROCESS_INFORMATION& p) {
            return std::find_if(terminatedProcesses.begin(), terminatedProcesses.end(), [&](const PROCESS_INFORMATION& t) { return t.dwProcessId == p.dwProcessId; }) != terminatedProcesses.end();
            }), childProcesses.end());
    }
}
void UpdateProcessList() {
    system("cls");
    std::cout << "List of processes:" << '\n';
    int i = 0;
    for (const auto& pi : childProcesses) {
        i++;
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        std::string status = (exitCode == STILL_ACTIVE) ? "Executes" : "Completed";
        std::cout << "PID: " << pi.dwProcessId << ", Status: " << status << "\n\n";
    }
    if (i == 0) {
        std::cout << "Empty list\n\n";
    }
}

int main() {
    system("color F0");
    SetConsoleOutputCP(CP_UTF8);
    int choice;
    do {
        std::cout << "1. Start a child process" << '\n';
        std::cout << "2. Update the list of processes" << '\n';
        std::cout << "3. Terminate all child processes" << '\n';
        std::cout << "4. Change Descriptor Saving Mode" << '\n';
        std::cout << "0. Exit" << '\n';
        std::cout << "Choose the Option: ";
        std::cin >> choice;

        switch (choice) {
        case 1:
            StartChildProcess();
            break;
        case 2:
            UpdateProcessList();
            break;
        case 3:
            TerminateChildProcesses();
            break;
        case 4:
            keepHandles = !keepHandles;
            std::cout << "Descriptor Persistence Mode: " << (keepHandles ? "On" : "Off") << "\n\n";
            break;
        case 0:
            break;
        default:
            std::cout << "Incorrect choice" << "\n\n";
        }

    } while (choice != 0);

    TerminateChildProcesses();

    return 0;
}