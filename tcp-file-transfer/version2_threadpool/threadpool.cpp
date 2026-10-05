#include "threadpool.h"

#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <iostream>

#include "../common/net.h"

// Defined in server.cpp: handles one connection (download or upload).
extern void handle_client(int client_socket);

using Clock = std::chrono::steady_clock;

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(&ThreadPool::worker, this);
}

void ThreadPool::enqueue(int client_socket) {
    size_t qlen;
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push({client_socket, Clock::now()});
        qlen = tasks.size();
    }
    condition.notify_one();
    log_msg("ENQUEUE fd=" + std::to_string(client_socket) + " queue_len=" + std::to_string(qlen) +
            " active_workers=" + std::to_string(active.load()));
}

size_t ThreadPool::queue_length() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
}

size_t ThreadPool::pending() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size() + size_t(active.load());
}

double ThreadPool::avg_service_ms() const {
    uint64_t n = served_count.load();
    return n ? total_service_us.load() / 1000.0 / n : 0.0;
}

double ThreadPool::avg_queue_wait_ms() const {
    uint64_t n = served_count.load();
    return n ? total_wait_us.load() / 1000.0 / n : 0.0;
}

void ThreadPool::worker() {
    while (true) {
        Task task;
        size_t qlen;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty())
                return;

            task = tasks.front();
            tasks.pop();
            qlen = tasks.size();
            active++;  // increment while holding the lock so pending() never undercounts
        }

        auto start = Clock::now();
        uint64_t wait_us = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(start - task.enqueued_at).count());
        log_msg("DEQUEUE fd=" + std::to_string(task.client_socket) + " waited=" +
                std::to_string(wait_us / 1000) + "ms queue_len=" + std::to_string(qlen) +
                " active_workers=" + std::to_string(active.load()));

        handle_client(task.client_socket);
        close(task.client_socket);

        uint64_t service_us = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
        total_service_us += service_us;
        total_wait_us += wait_us;
        served_count++;
        active--;

        char line[160];
        snprintf(line, sizeof(line), "METRICS service=%.1fms avg_service=%.1fms avg_queue_wait=%.1fms served=%llu active=%d",
                 service_us / 1000.0, avg_service_ms(), avg_queue_wait_ms(),
                 (unsigned long long)served_count.load(), active.load());
        log_msg(line);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    for (std::thread& worker : workers)
        worker.join();
}
