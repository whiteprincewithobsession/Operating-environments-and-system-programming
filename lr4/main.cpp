#include <windows.h>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>

const int MAX_PHILOSOPHERS = 10;

struct Philosopher {
    int id;
    HANDLE leftFork;
    HANDLE rightFork;
    int eatingCount;
    int thinkingTime;
    int eatingTime;
    LONGLONG totalActiveTime;
    LONGLONG totalBlockedTime;
};

struct Config {
    int numPhilosophers;
    int simulationTime;
    int minThinkingTime;
    int maxThinkingTime;
    int minEatingTime;
    int maxEatingTime;
    int timeout;
};

Config config;
std::vector<Philosopher> philosophers;
std::vector<HANDLE> forks;
HANDLE printMutex;
bool running = true;

void printStatus(const char* status, int id) {
    WaitForSingleObject(printMutex, INFINITE);
    std::cout << "Philosopher " << id << " " << status << std::endl;
    ReleaseMutex(printMutex);
}

int getRandomTime(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

DWORD WINAPI philosopherThread(LPVOID param) {
    Philosopher* phil = (Philosopher*)param;
    LARGE_INTEGER startTime, endTime, frequency;
    QueryPerformanceFrequency(&frequency);

    while (running) {
        int thinkingTime = getRandomTime(config.minThinkingTime, config.maxThinkingTime);
        printStatus("is thinking", phil->id);
        Sleep(thinkingTime);

        QueryPerformanceCounter(&startTime);

        HANDLE handles[2] = { phil->leftFork, phil->rightFork };
        DWORD result = WaitForMultipleObjects(2, handles, TRUE, config.timeout);

        if (result == WAIT_OBJECT_0) {
            QueryPerformanceCounter(&endTime);
            phil->totalBlockedTime += (endTime.QuadPart - startTime.QuadPart) * 1000 / frequency.QuadPart;

            int eatingTime = getRandomTime(config.minEatingTime, config.maxEatingTime);
            printStatus("is eating", phil->id);
            Sleep(eatingTime);
            phil->eatingCount++;

            QueryPerformanceCounter(&startTime);
            phil->totalActiveTime += eatingTime;

            ReleaseMutex(phil->leftFork);
            ReleaseMutex(phil->rightFork);

            QueryPerformanceCounter(&endTime);
            phil->totalActiveTime += (endTime.QuadPart - startTime.QuadPart) * 1000 / frequency.QuadPart;
        }
        else {
            QueryPerformanceCounter(&endTime);
            phil->totalBlockedTime += config.timeout;
            printStatus("couldn't acquire forks", phil->id);
        }
    }

    return 0;
}

void runSimulation() {
    std::vector<HANDLE> threads;

    for (int i = 0; i < config.numPhilosophers; i++) {
        HANDLE thread = CreateThread(NULL, 0, philosopherThread, &philosophers[i], 0, NULL);
        threads.push_back(thread);
    }

    Sleep(config.simulationTime * 1000);
    running = false;

    WaitForMultipleObjects(config.numPhilosophers, threads.data(), TRUE, INFINITE);

    for (HANDLE thread : threads) {
        CloseHandle(thread);
    }
}

void printResults() {
    std::cout << "\nSimulation Results:\n";
    std::cout << "-------------------\n";

    int totalEatingCount = 0;
    LONGLONG totalActiveTime = 0;
    LONGLONG totalBlockedTime = 0;

    for (const auto& phil : philosophers) {
        std::cout << "Philosopher " << phil.id << ":\n";
        std::cout << "  Eating count: " << phil.eatingCount << "\n";
        std::cout << "  Active time: " << phil.totalActiveTime << " ms\n";
        std::cout << "  Blocked time: " << phil.totalBlockedTime << " ms\n";
        std::cout << "  Active/Blocked ratio: " << (double)phil.totalActiveTime / phil.totalBlockedTime << "\n\n";

        totalEatingCount += phil.eatingCount;
        totalActiveTime += phil.totalActiveTime;
        totalBlockedTime += phil.totalBlockedTime;
    }

    double avgEatingCount = (double)totalEatingCount / config.numPhilosophers;
    double avgActiveTime = (double)totalActiveTime / config.numPhilosophers;
    double avgBlockedTime = (double)totalBlockedTime / config.numPhilosophers;

    std::cout << "Overall Statistics:\n";
    std::cout << "  Average eating count: " << avgEatingCount << "\n";
    std::cout << "  Average active time: " << avgActiveTime << " ms\n";
    std::cout << "  Average blocked time: " << avgBlockedTime << " ms\n";
    std::cout << "  Overall active/blocked ratio: " << (double)totalActiveTime / totalBlockedTime << "\n";
    std::cout << "  Throughput: " << (double)totalEatingCount / (config.simulationTime) << " meals/second\n";
}

int main() {
    config.numPhilosophers = 5;
    config.simulationTime = 15;  
    config.minThinkingTime = 1000; 
    config.maxThinkingTime = 3000;  
    config.minEatingTime = 1000;
    config.maxEatingTime = 3000;
    config.timeout = 5000; 

    for (int i = 0; i < config.numPhilosophers; i++) {
        forks.push_back(CreateMutex(NULL, FALSE, NULL));
    }

    for (int i = 0; i < config.numPhilosophers; i++) {
        Philosopher phil;
        phil.id = i;
        phil.leftFork = forks[i];
        phil.rightFork = forks[(i + 1) % config.numPhilosophers];
        phil.eatingCount = 0;
        phil.totalActiveTime = 0;
        phil.totalBlockedTime = 0;
        philosophers.push_back(phil);
    }

    printMutex = CreateMutex(NULL, FALSE, NULL);

    std::cout << "Starting simulation for " << config.simulationTime << " seconds...\n";
    runSimulation();

    printResults();

    for (HANDLE fork : forks) {
        CloseHandle(fork);
    }
    CloseHandle(printMutex);

    return 0;
}