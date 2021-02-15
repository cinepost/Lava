#ifndef SRC_FALCOR_UTILS_THREADPOOL_H_
#define SRC_FALCOR_UTILS_THREADPOOL_H_

#include <iostream>
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <random>

#include "Falcor/Core/Framework.h"

namespace Falcor {

class dlldecl ThreadPool {
  public:
    ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
  private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threadCount) : stop(false) {
    for(size_t i = 0 ; i < threadCount; ++i) {
        workers.emplace_back([this]{
            for(;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                    
                    if(this->stop && this->tasks.empty())
                        return;
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
    }
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("Task enqueue on stopped ThreadPool !!!");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for(std::thread &worker: workers)
        worker.join();
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_THREADING_H_
