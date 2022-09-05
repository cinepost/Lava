#ifndef SRC_FALCOR_UTILS_THREADPOOL_H_
#define SRC_FALCOR_UTILS_THREADPOOL_H_

#include <thread>
#include "thread-pool-3.3.0/BS_thread_pool.hpp"

#include "Falcor/Core/Framework.h"


namespace Falcor {

class dlldecl ThreadPool: public BS::thread_pool {
  public:
    ThreadPool(): BS::thread_pool(std::max(1u, std::thread::hardware_concurrency() - 1)){};

    static ThreadPool& instance() {
        static ThreadPool instance;
        return instance;
    }

  private:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_THREADING_H_
