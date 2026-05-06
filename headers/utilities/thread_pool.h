#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
  public:
    ThreadPool(unsigned int threadCount = 2);
    ~ThreadPool();

    void enqueue(std::function<void()> task);
    void stopAllThreads();

  private:
    std::atomic<bool> stop_{false};
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable workCondition_;
};