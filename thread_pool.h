#pragma once

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <string>

class ThreadPool {

public:
    ~ThreadPool(){
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all();

        for (auto &thread : threads)
            thread.join();
    }

    ThreadPool(unsigned int numThreads) : stop(false) {
        threads.reserve(numThreads);

        for (unsigned int i = 0; i < numThreads; i++) {
            threads.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    //Lock the queue, get task
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);

                        condition.wait(lock, [this] { return !tasks.empty() || stop; });

                        if (stop && tasks.empty())
                            return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                }
                }); 
        }
    }

    void enqueue(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(queueMutex);

        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.push(task);
        condition.notify_one();
    }

private:
    bool stop;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
};