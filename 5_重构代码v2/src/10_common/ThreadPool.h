#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <type_traits>
#include <iostream>

/**
 * 轻量级线程池
 *
 * 用法：
 *   ThreadPool pool(4);  // 4个线程
 *   auto task = pool.enqueue([](int a, int b) { return a + b; }, 1, 2);
 *   int result = task.get();
 */
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
    std::atomic<int> activeWorkers{0};
    std::mutex active_mutex;
    std::condition_variable active_cv;

public:
    explicit ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; i++) {
            workers.emplace_back([this, i]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this]() {
                            return stop.load() || !tasks.empty();
                        });
                        if (stop.load() && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    {
                        std::lock_guard<std::mutex> lock(active_mutex);
                        activeWorkers++;
                    }
                    task();
                    {
                        std::lock_guard<std::mutex> lock(active_mutex);
                        activeWorkers--;
                    }
                    active_cv.notify_one();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop.store(true);
        }
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }

    /**
     * 提交任务
     * @tparam F 函数类型
     * @tparam Args 参数类型
     * @return std::future<返回值类型>
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using return_type = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stop.load())
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }

    /** 等待所有任务完成 */
    void waitAll() {
        std::unique_lock<std::mutex> lock(active_mutex);
        active_cv.wait(lock, [this]() {
            std::lock_guard<std::mutex> qlock(queue_mutex);
            return tasks.empty() && activeWorkers == 0;
        });
    }

    size_t pending() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return tasks.size();
    }

    size_t active() {
        return activeWorkers.load();
    }

    size_t size() const {
        return workers.size();
    }
};

#endif // THREAD_POOL_H
