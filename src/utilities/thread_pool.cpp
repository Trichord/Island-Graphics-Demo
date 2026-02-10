#include "thread_pool.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>

ThreadPool::ThreadPool(unsigned int desiredThreadCount) : stop{false}
{
    unsigned int threadsAvailable{std::thread::hardware_concurrency()};
    unsigned int maxThreads{std::min(desiredThreadCount, threadsAvailable)};

    threads.reserve(maxThreads);

    for (unsigned int i = 0; i < maxThreads; ++i)
    {
        threads.emplace_back([this] {
            while (true)
            {
                std::function<void()> task;

                // Lock the queue, get task
                {
                    std::unique_lock<std::mutex> lock{queueMutex};

                    workCondition.wait(lock, [this] { return !tasks.empty() || stop; });

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

ThreadPool::~ThreadPool()
{
    stopAllThreads();
}

void ThreadPool::enqueue(std::function<void()> task)
{
    std::unique_lock<std::mutex> lock{queueMutex};

    if (stop)
        throw std::runtime_error{"enqueue on stopped ThreadPool"};

    tasks.push(task);
    workCondition.notify_one();
}

void ThreadPool::stopAllThreads()
{
    {
        std::unique_lock<std::mutex> lock{queueMutex};
        stop = true;
        workCondition.notify_all();
    }

    for (auto &thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }
}