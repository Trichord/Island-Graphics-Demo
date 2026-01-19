#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  ThreadPool(unsigned int desiredThreadCount);
  ~ThreadPool();

  void enqueue(std::function<void()> task);
  void stopAllThreads();

 private:
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;
  std::queue<std::function<void()>> tasks;
  std::mutex queueMutex;
  std::condition_variable workCondition;
};