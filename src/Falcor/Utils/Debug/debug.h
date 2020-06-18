#ifndef __FALCOR_UTILS_DEBUG_H__
#define __FALCOR_UTILS_DEBUG_H__

#include <atomic>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifdef DEBUG
#define LOG_DEBUG 1
#else
#define LOG_DEBUG 0
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

#endif // __FALCOR_UTILS_DEBUG_H__