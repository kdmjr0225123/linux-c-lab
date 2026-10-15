/*
 * Khalil Manigaulte - HCSC-375-01 Operating Systems - Prof. Sazegar
 * Lab 6, Exercise 6: Bounded Buffer with Condition Variables
 *
 * Compile: g++ -o bounded_buffer bounded_buffer.cpp -pthread -std=c++11
 * Run:     ./bounded_buffer
 */
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <iostream>
#include <cstdlib>

#define BUFFER_SIZE 5
#define NUM_ITEMS 10

std::queue<int> buffer;
std::mutex mtx;
std::condition_variable not_full, not_empty;

void producer() {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [] { return buffer.size() < BUFFER_SIZE; });
        int item = rand() % 100;
        buffer.push(item);
        std::cout << "Produced: " << item << "\n";
        not_empty.notify_one();
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void consumer() {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [] { return !buffer.empty(); });
        int item = buffer.front();
        buffer.pop();
        std::cout << "Consumed: " << item << "\n";
        not_full.notify_one();
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

int main() {
    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();
    return 0;
}
