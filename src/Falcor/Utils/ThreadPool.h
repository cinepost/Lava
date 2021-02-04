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
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
  
  private:
    std::vector< std::thread > workers;
    std::queue< std::function<void()> > tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    //std::condition_variable cv_task;
    //std::condition_variable cv_finished;
    //std::atomic_uint processed;
    //unsigned int busy;
    bool stop;

};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_THREADING_H_
