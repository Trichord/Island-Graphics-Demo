#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class ThreadPool {
 public:
  ThreadPool();
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