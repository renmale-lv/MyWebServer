#ifndef LOG_HPP
#define LOG_HPP

#include <mutex>
#include <cstring>
#include <thread>
#include <string>
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "../buffer/buffer.hpp"
#include "./blockqueue.hpp"

// 单例日志类
class Log
{
public:
    // 初始化
    void init(int level,
              const char *path = "./log",
              const char *suffix = "./log",
              int maxQueueCapacity = 1024);

    // 单例
    static Log *Instance();

    // 异步线程工作函数
    static void FlushLogThread();

    // 写日志
    void write(int level, const char *format, ...);

    // 立刻将（系统）缓冲区中的数据写到日志文件中
    void flush();

    // 获得当前日志等级
    int GetLevel();

    // 设置日志等级
    void SetLevel(int level);

    // 日志系统是否打开
    bool IsOpen();

private:
    Log();

    // 写入日志级别
    void AppendLogLevelTitle_(int level);

    virtual ~Log();

    // 异步写
    void AsyncWrite_();

private:
    // 日志文件路径长度
    static const int LOG_PATH_LEN;

    // 日志文件名长度
    static const int LOG_NAME_LEN;

    // 一个日志文件最多可容纳的日志条数
    static const int MAX_LINES;

    // 日志文件路径
    const char *path_;

    // 日志文件后缀
    const char *suffix_;

    int MAX_LINES_;

    // 当前日志文件以写入的日志数
    int lineCount_;

    // 今日日期
    int toDay_;

    // 日志系统是否打开
    bool isOpen_;

    // 缓冲区
    Buffer buff_;

    // 日志屏蔽等级，等级比level低才会写进日志
    int level_;

    // 日志是否异步
    bool isAsync_;

    // 日志文件指针
    FILE *fp_;

    std::unique_ptr<BlockDeque<std::string>> deque_;

    // 日志线程
    std::unique_ptr<std::thread> writeThread_;

    std::mutex mtx_;
};

#define LOG_BASE(level, format, ...)                   \
    do                                                 \
    {                                                  \
        Log *log = Log::Instance();                    \
        if (log->IsOpen() && log->GetLevel() <= level) \
        {                                              \
            log->write(level, format, ##__VA_ARGS__);  \
            log->flush();                              \
        }                                              \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do                                     \
    {                                      \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_INFO(format, ...)              \
    do                                     \
    {                                      \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_WARN(format, ...)              \
    do                                     \
    {                                      \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_ERROR(format, ...)             \
    do                                     \
    {                                      \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif