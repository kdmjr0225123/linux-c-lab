/*
 * Khalil Manigaulte - HCSC-375-01 Operating Systems - Prof. Sazegar
 * Lab 6, Exercise 2: Bank Account Simulation with Mutex and Condition Variables
 *
 * Compile: g++ -o bank_account bank_account.cpp -pthread -std=c++11
 * Run:     ./bank_account
 */
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <iostream>

class BankAccount {
private:
    int balance;
    std::mutex mtx;
    std::condition_variable cv;
public:
    BankAccount(int initial_balance) : balance(initial_balance) {}

    void deposit(int amount) {
        std::unique_lock<std::mutex> lock(mtx);
        balance += amount;
        std::cout << "Deposited: " << amount << ", New Balance: " << balance << "\n";
        cv.notify_all();
    }

    void withdraw(int amount) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return balance >= amount; });
        balance -= amount;
        std::cout << "Withdrew: " << amount << ", New Balance: " << balance << "\n";
    }
};

void user_transactions(BankAccount &account) {
    account.deposit(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    account.withdraw(50);
}

int main() {
    BankAccount account(100);
    std::vector<std::thread> users;
    for (int i = 0; i < 5; ++i) {
        users.push_back(std::thread(user_transactions, std::ref(account)));
    }
    for (auto &t : users) {
        t.join();
    }
    return 0;
}
