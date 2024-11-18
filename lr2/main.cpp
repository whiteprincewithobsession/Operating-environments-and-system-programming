#include <iostream>
#include <Windows.h>
#include <future>
#include <chrono>
#include <cstdlib>
#include <vector>

const LONGLONG FILE_SIZE = 1LL * 1024 * 1024 * 1024;
//const LONGLONG FILE_SIZE = 1LL * 1024 * 1024;

void CreateLargeFile(const std::wstring& filePath);
void ProcessFileAsync(const std::wstring& filePath);
void ProcessFileSync(const std::wstring& filePath);
void ProcessFileMultiThreaded(const std::wstring& filePath);

int main()
{
    const std::wstring filePath = L"large_file.bin";
    const int iterations = 10;

    CreateLargeFile(filePath);
    std::cout << "Each file processing method will be run 10 times to average the result\n\n\n";
    std::cout << "Asynchronous file processing using in-memory file mapping:" << '\n';
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++)
    {
        ProcessFileAsync(filePath);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / (double)iterations;
    std::cout << "Average time: " << duration << " ms" << '\n' << '\n';

    std::cout << "Sync file processing:" << '\n';
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++)
    {
        ProcessFileSync(filePath);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / (double)iterations;
    std::cout << "Average time: " << duration << " ms" << '\n' << '\n';

    std::cout << "Multi-threaded file processing:" << '\n';
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++)
    {
        ProcessFileMultiThreaded(filePath);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / (double)iterations;
    std::cout << "Average time: " << duration << " ms" << '\n';

    return 0;
}

void CreateLargeFile(const std::wstring& filePath)
{
    HANDLE fileHandle = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        std::cout << "File creating error" << '\n';
        return;
    }

    char* buffer = new char[1024 * 1024];
    DWORD bytesWritten;
    LONGLONG bytesRemaining = FILE_SIZE;

    for (int i = 0; i < 1024 * 1024; i++)
    {
        buffer[i] = static_cast<char>(rand());
    }

    while (bytesRemaining > 0)
    {
        DWORD bytesToWrite = static_cast<DWORD>(min(bytesRemaining, 1024LL * 1024));
        if (!WriteFile(fileHandle, buffer, bytesToWrite, &bytesWritten, NULL))
        {
            std::cout << "File input error" << '\n';
            CloseHandle(fileHandle);
            delete[] buffer;
            return;
        }

        bytesRemaining -= bytesWritten;
    }

    CloseHandle(fileHandle);
    delete[] buffer;
    std::cout << "Large file successfully created" << '\n';
}

void ProcessFileAsync(const std::wstring& filePath)
{
    HANDLE fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        std::cout << "Error when openning file" << '\n';
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize))
    {
        std::cout << "Error getting size of file" << '\n';
        CloseHandle(fileHandle);
        return;
    }

    HANDLE fileMappingHandle = CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (fileMappingHandle == NULL)
    {
        std::cout << "File display creation error" << '\n';
        CloseHandle(fileHandle);
        return;
    }

    LPVOID fileView = MapViewOfFile(fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (fileView == NULL)
    {
        std::cout << "Memory File Display Error" << '\n';
        CloseHandle(fileMappingHandle);
        CloseHandle(fileHandle);
        return;
    }

    UnmapViewOfFile(fileView);
    CloseHandle(fileMappingHandle);
    CloseHandle(fileHandle);
}

void ProcessFileSync(const std::wstring& filePath)
{
    HANDLE fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        std::cout << "File opening error" << '\n';
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize))
    {
        std::cout << "Error getting file size" << '\n';
        CloseHandle(fileHandle);
        return;
    }

    char* buffer = new char[fileSize.QuadPart];
    DWORD bytesRead;
    if (!ReadFile(fileHandle, buffer, static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL))
    {
        std::cout << "File Read Error" << '\n';
        delete[] buffer;
        CloseHandle(fileHandle);
        return;
    }


    delete[] buffer;
    CloseHandle(fileHandle);
}

void ProcessFileMultiThreaded(const std::wstring& filePath)
{
    HANDLE fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        std::cout << "File opening error" << '\n';
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize))
    {
        std::cout << "Error getting file size" << '\n';
        CloseHandle(fileHandle);
        return;
    }

    const int numThreads = 4;
    const LONGLONG chunkSize = fileSize.QuadPart / numThreads;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < numThreads; i++)
    {
        LONGLONG offset = i * chunkSize;
        LONGLONG size = (i == numThreads - 1) ? fileSize.QuadPart - offset : chunkSize;

        futures.push_back(async(std::launch::async, [=]() {
            HANDLE fileMapHandle = CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
            if (fileMapHandle == NULL)
            {
                std::cout << "File display creation error" << '\n';
                return;
            }

            LPVOID fileView = MapViewOfFile(fileMapHandle, FILE_MAP_READ, 0, offset, size);
            if (fileView == NULL)
            {
                std::cout << "Memory File Display Error" << '\n';
                CloseHandle(fileMapHandle);
                return;
            }

            char* fileViewPtr = static_cast<char*>(fileView);
            int charCount = 0;
            for (LONGLONG i = 0; i < size; i++)
            {
                if (fileViewPtr[i] != 0)
                {
                    charCount++;
                }
            }
            std::cout << "Num of symbols in range: " << charCount << "    Iteration:" << i + 1 << '\n';
            UnmapViewOfFile(fileView);
            CloseHandle(fileMapHandle);
            }));
    }

    for (auto& fut : futures)
    {
        fut.get();
    }

    CloseHandle(fileHandle);
}