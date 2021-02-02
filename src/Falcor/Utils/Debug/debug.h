#ifndef __FALCOR_UTILS_DEBUG_H__
#define __FALCOR_UTILS_DEBUG_H__

#include <atomic>
#include <string>
#include <chrono>
#include <iostream>


// simple colored console debug print macros
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifdef NDEBUG
#define LOG_DEBUG 1
#else
#define LOG_DEBUG 1
#endif

extern std::atomic_uint32_t _dbg_i;

#define LOG_FTL(fmt, ...) \
        do { if (LOG_DEBUG) fprintf(stderr, ANSI_COLOR_RED "%s:%d:%s(): " fmt "\n" ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define LOG_ERR(fmt, ...) \
        do { if (LOG_DEBUG) fprintf(stderr, ANSI_COLOR_RED "%s:%d:%s(): " fmt "\n" ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define LOG_INFO(fmt, ...) \
        do { if (LOG_DEBUG) fprintf(stdout, ANSI_COLOR_BLUE "%s:%d:%s(): " fmt "\n" ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define LOG_WARN(fmt, ...) \
        do { if (LOG_DEBUG) fprintf(stdout, ANSI_COLOR_YELLOW "%s:%d:%s(): " fmt "\n" ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

#define LOG_DBG(fmt, ...) \
        do { if (LOG_DEBUG) fprintf(stdout, ANSI_COLOR_WHITE "%s:%d:%s(): " fmt "\n" ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)


// block execution time profiler
struct blk_exec_time_profiler {
    std::string name;
    std::chrono::high_resolution_clock::time_point p;
    blk_exec_time_profiler(std::string const &n) :
        name(n), p(std::chrono::high_resolution_clock::now()) { }
    ~blk_exec_time_profiler()
    {
        using dura = std::chrono::duration<double>;
        auto d = std::chrono::high_resolution_clock::now() - p;
        std::cout << name << ": " << std::chrono::duration_cast<dura>(d).count() << std::endl;
    }
};

#ifdef NDEBUG
#define BLK_TIME_DEBUG 0
#else
#define BLK_TIME_DEBUG 1
#endif

#define TIME_PROFILE_BLOCK(pbn) \
        do{ if (BLK_TIME_DEBUG) blk_exec_time_profiler _pfinstance(pbn); } while (0)

// Record the execution time of some code, in milliseconds.
#define DECLARE_TIMING(s)  int64_t timeStart_##s; double timeDiff_##s; double timeTally_##s = 0; int countTally_##s = 0
#define START_TIMING(s)    timeStart_##s = cvGetTickCount()
#define STOP_TIMING(s)     timeDiff_##s = (double)(cvGetTickCount() - timeStart_##s); timeTally_##s += timeDiff_##s; countTally_##s++
#define GET_TIMING(s)      (double)(timeDiff_##s / (cvGetTickFrequency()*1000.0))
#define GET_AVERAGE_TIMING(s)   (double)(countTally_##s ? timeTally_##s/ ((double)countTally_##s * cvGetTickFrequency()*1000.0) : 0)
#define CLEAR_AVERAGE_TIMING(s) timeTally_##s = 0; countTally_##s = 0

#endif // __FALCOR_UTILS_DEBUG_H__