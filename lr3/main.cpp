#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <memory>
#include <functional>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <barrier>

std::mutex cout_mutex;

template<typename T>
void synchronized_cout(T&& t) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << std::forward<T>(t) << std::endl;
}


class SharedBuffer {
public:
    explicit SharedBuffer(size_t size) : data(size) {}

    void writeData(const std::string& message) {
        std::copy(message.begin(), message.end(), data.begin());
    }

    std::string readData() const {
        return std::string(data.begin(), std::find(data.begin(), data.end(), '\0'));
    }

private:
    std::vector<char> data;
};

class BufferPool {
public:
    BufferPool(size_t bufferSize, size_t poolSize)
        : bufferSize(bufferSize), poolSize(poolSize) {
        for (size_t i = 0; i < poolSize; ++i) {
            returnBuffer(std::make_unique<SharedBuffer>(bufferSize));
        }
    }

    std::unique_ptr<SharedBuffer> getBuffer() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !availableBuffers.empty(); });
        auto buffer = std::move(availableBuffers.front());
        availableBuffers.pop();
        return buffer;
    }

    void returnBuffer(std::unique_ptr<SharedBuffer> buffer) {
        std::lock_guard<std::mutex> lock(mutex);
        availableBuffers.push(std::move(buffer));
        cv.notify_one();
    }

private:
    std::queue<std::unique_ptr<SharedBuffer>> availableBuffers;
    std::mutex mutex;
    std::condition_variable cv;
    const size_t bufferSize;
    const size_t poolSize;
};

class Barrier {
public:
    explicit Barrier(std::size_t producerCount, std::size_t consumerCount)
        : producerThreshold(producerCount), consumerThreshold(consumerCount),
        producerCount(producerCount), consumerCount(consumerCount), generation(0) {}

    void waitProducer() {
        std::unique_lock<std::mutex> lock(mutex);
        auto gen = generation;
        if (--producerCount == 0) {
            generation++;
            producerCount = producerThreshold;
            cv.notify_all();
        }
        else {
            cv.wait(lock, [this, gen] { return gen != generation; });
        }
    }

    void waitConsumer() {
        std::unique_lock<std::mutex> lock(mutex);
        auto gen = generation;
        if (--consumerCount == 0) {
            generation++;
            consumerCount = consumerThreshold;
            cv.notify_all();
        }
        else {
            cv.wait(lock, [this, gen] { return gen != generation; });
        }
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    std::size_t producerThreshold;
    std::size_t consumerThreshold;
    std::size_t producerCount;
    std::size_t consumerCount;
    std::size_t generation;
};

class Process {
public:
    using ProcessFunction = std::function<void(SharedBuffer&, const std::string&, Barrier&)>;

    Process(BufferPool& pool, ProcessFunction func, std::string name, Barrier& barrier)
        : bufferPool(pool), processFunc(std::move(func)), processName(std::move(name)), syncBarrier(barrier) {}

    void run() {
        auto buffer = bufferPool.getBuffer();
        synchronized_cout(processName + " got buffer");
        std::this_thread::sleep_for(std::chrono::milliseconds(250 + rand() % 100));

        processFunc(*buffer, processName, syncBarrier);

        synchronized_cout(processName + " returning buffer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + rand() % 70));

        bufferPool.returnBuffer(std::move(buffer));
    }

private:
    BufferPool& bufferPool;
    ProcessFunction processFunc;
    std::string processName;
    Barrier& syncBarrier;
};

std::string generateRandomMessage() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 100);

    std::ostringstream oss;
    oss << "Message-" << std::setfill('0') << std::setw(3) << dis(gen);
    return oss.str();
}

int main() {
    BufferPool pool(1024, 5);

    std::vector<std::unique_ptr<Process>> processes;
    std::vector<std::thread> threads;

    std::atomic<int> counter{ 0 };
    const int numProducers = 5;
    const int numConsumers = 5;
    Barrier syncBarrier(numProducers, numConsumers);

    auto createProducer = [&](const std::string& name) {
        processes.push_back(std::make_unique<Process>(
            pool,
            [&counter](SharedBuffer& buffer, const std::string& name, Barrier& barrier) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 250));
                std::string message = generateRandomMessage();
                buffer.writeData(message);
                synchronized_cout(name + " wrote: " + message);
                counter++;
                barrier.waitProducer();
            },
            name,
            syncBarrier
        ));
        threads.emplace_back(&Process::run, processes.back().get());
        };

    auto createConsumer = [&](const std::string& name) {
        processes.push_back(std::make_unique<Process>(
            pool,
            [&counter](SharedBuffer& buffer, const std::string& name, Barrier& barrier) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150 + rand() % 250));
                barrier.waitConsumer();
                std::string message = buffer.readData();
                synchronized_cout(name + " read:  " + message);
                counter++;
            },
            name,
            syncBarrier
        ));
        threads.emplace_back(&Process::run, processes.back().get());
        };
    for (int i = 0; i < numProducers; ++i) {
        createProducer("Producer-" + std::to_string(i));
    }
    for (int i = 0; i < numConsumers; ++i) {
        createConsumer("Consumer-" + std::to_string(i));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    synchronized_cout("Total operations completed: " + std::to_string(counter));

    return 0;
}