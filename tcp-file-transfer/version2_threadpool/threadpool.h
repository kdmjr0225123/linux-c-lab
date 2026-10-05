#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    void enqueue(int client_socket);

    // Challenge 3: metrics
    size_t queue_length();
    int active_workers() const { return active.load(); }
    size_t pending();  // queued + in service (used for the connection cap)
    uint64_t served() const { return served_count.load(); }
    double avg_service_ms() const;
    double avg_queue_wait_ms() const;

private:
    void worker();

    struct Task {
        int client_socket;
        std::chrono::steady_clock::time_point enqueued_at;
    };

    std::vector<std::thread> workers;
    std::queue<Task> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

    std::atomic<int> active{0};
    std::atomic<uint64_t> served_count{0};
    std::atomic<uint64_t> total_service_us{0};
    std::atomic<uint64_t> total_wait_us{0};
};

#endif
