#ifndef HSLL_LOG
#define HSLL_LOG

#include <mutex>
#include <chrono>
#include <fstream>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#define LOG_CREATEDIRECTORY CreateDirectoryA("ServerLog", NULL)

#elif defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#define LOG_CREATEDIRECTORY mkdir("ServerLog", 0755)
#endif

namespace HSLL
{

#define CONFIG_PATH_MAX_LENGTH 256

    class Log
    {
    private:
        std::mutex mtx;
        std::fstream fstream;
        bool mulThread;
        unsigned int maxSize;
        unsigned int fileSize;
        char logPath[CONFIG_PATH_MAX_LENGTH];

    public:
        ~Log();

        Log(bool mulThread, unsigned int maxSize = 4294967295);

        Log(const char *directoryPath, bool mulThread, unsigned int maxSize = 4294967295);

        bool Init();

        void WriteLog(const char *str);
    };
}

#ifndef FALSE_RETURN
#define FALSE_RETURN(condition) \
    if (!(condition))           \
        return false;
#endif

#ifndef FALSE_LOG
#define FALSE_LOG(condition, pLog, str) \
    if (!(condition))                   \
    {                                   \
        if (pLog)                       \
            pLog->WriteLog(str);        \
    }
#endif

#ifndef FALSE_PRINT_RETURN
#define FALSE_PRINT_RETURN(condition, str) \
    if (!(condition))               \
    {                               \
        printf(str);                \
        return false;               \
    }
#endif

#ifndef FALSE_LOG_RETURN
#define FALSE_LOG_RETURN(condition, pLog, str) \
    if (!(condition))                          \
    {                                          \
        if (pLog)                              \
            pLog->WriteLog(str);               \
        return false;                          \
    }
#endif

#ifndef FALSE_LOG_RETURN_FUNC
#define FALSE_LOG_RETURN_FUNC(condition, func, pLog, str) \
    if (!(condition))                                     \
    {                                                     \
        if (pLog)                                         \
            pLog->WriteLog(str);                          \
        func;                                             \
        return false;                                     \
    }
#endif

#ifndef FALSE_LOG_THROW
#define FALSE_LOG_THROW(condition, pLog, str) \
    if (!(condition))                         \
    {                                         \
        if (pLog)                             \
            pLog->WriteLog(str);              \
        throw;                                \
    }
#endif

#endif
