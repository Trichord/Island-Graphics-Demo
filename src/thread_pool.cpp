#include "../headers/thread_pool.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

ThreadPool::ThreadPool() : stop(false) {
  unsigned int maxThreads = std::thread::hardware_concurrency();

  // Cap at 6 threads for PBR texture loading.
  unsigned int numThreads = std::min(6u, maxThreads == 0 ? 2 : maxThreads / 2);
  threads.reserve(numThreads);

  for (unsigned int i = 0; i < numThreads; i++) {
    threads.emplace_back([this] {
      while (true) {
        std::function<void()> task;

        // Lock the queue, get task
        {
          std::unique_lock<std::mutex> lock(queueMutex);

          workCondition.wait(lock, [this] { return !tasks.empty() || stop; });

          if (stop && tasks.empty()) return;

          task = std::move(tasks.front());
          tasks.pop();
        }

        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() { stopAllThreads(); }

void ThreadPool::enqueue(std::function<void()> task) {
  std::unique_lock<std::mutex> lock(queueMutex);

  if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

  tasks.push(task);
  workCondition.notify_one();
}

void ThreadPool::stopAllThreads() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
    workCondition.notify_all();
  }

  for (auto& thread : threads) {
    if (thread.joinable()) thread.join();
  }
}