/*
 * Khalil Manigaulte - HCSC-375-01 Operating Systems - Prof. Sazegar
 * Lab 6, Exercise 4: Dining Philosophers with Mutexes
 *
 * Compile: g++ -o dining_philosophers dining_philosophers.cpp -pthread -std=c++11
 * Run:     ./dining_philosophers
 */
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <iostream>
#include <utility>

#define NUM_PHILOSOPHERS 5

std::mutex forks[NUM_PHILOSOPHERS];

void philosopher(int id) {
    int left = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;
    if (id == NUM_PHILOSOPHERS - 1) { // Avoid deadlock
        std::swap(left, right);
    }

    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        forks[left].lock();
        forks[right].lock();
        std::cout << "Philosopher " << id << " is eating.\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        forks[right].unlock();
        forks[left].unlock();
    }
}

int main() {
    std::vector<std::thread> philosophers;
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers.push_back(std::thread(philosopher, i));
    }
    for (auto &t : philosophers) {
        t.join();
    }
    return 0;
}
