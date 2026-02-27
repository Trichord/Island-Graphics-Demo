#include "thread_pool.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>

ThreadPool::ThreadPool(unsigned int desiredThreadCount) : stop_{false}
{
    unsigned int threadsAvailable{std::thread::hardware_concurrency()};
    unsigned int maxThreads{std::min(desiredThreadCount, threadsAvailable)};

    threads_.reserve(maxThreads);

    for (unsigned int i = 0; i < maxThreads; ++i)
    {
        threads_.emplace_back([this] {
            while (true)
            {
                std::function<void()> task;

                // Lock the queue, get task
                {
                    std::unique_lock<std::mutex> lock{queueMutex_};

                    workCondition_.wait(lock, [this] { return !tasks_.empty() || stop_; });

                    if (stop_ && tasks_.empty())
                        return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                task();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    stopAllThreads();
}

void ThreadPool::enqueue(std::function<void()> task)
{
    std::unique_lock<std::mutex> lock{queueMutex_};

    if (stop_)
        throw std::runtime_error{"enqueue on stopped ThreadPool"};

    tasks_.push(task);
    workCondition_.notify_one();
}

void ThreadPool::stopAllThreads()
{
    {
        std::unique_lock<std::mutex> lock{queueMutex_};
        stop_ = true;
        workCondition_.notify_all();
    }

    for (auto &thread : threads_)
    {
        if (thread.joinable())
            thread.join();
    }
}