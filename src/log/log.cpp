#include "log.hpp"

const int Log::LOG_PATH_LEN = 256;
const int Log::LOG_NAME_LEN = 256;
const int Log::MAX_LINES = 50000;

Log::Log()
{
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

int Log::GetLevel()
{
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level)
{
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}

void Log::flush()
{
    if (isAsync_)
    {
        deque_->flush();
    }
    // 将缓冲区的内容写到fp_所指文件中，而不用等待程序结束
    fflush(fp_);
}

void Log::AsyncWrite_()
{
    std::string str = "";
    while (deque_->pop(str))
    {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

void Log::FlushLogThread()
{
    Log::Instance()->AsyncWrite_();
}

void Log::init(int level = 1, const char *path, const char *suffix, int maxQueueSize)
{
    isOpen_ = true;
    level_ = level;
    // 如果请求数大于0，则使用异步写
    if (maxQueueSize > 0)
    {
        isAsync_ = true;
        if (!deque_)
        {
            std::unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = std::move(newDeque);

            // 实例化异步线程
            std::unique_ptr<std::thread> NewThread(new std::thread(FlushLogThread));
            writeThread_ = std::move(NewThread);
        }
    }
    else
    {
        isAsync_ = false;
    }

    lineCount_ = 0;

    // 获取当前时间
    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;

    // 生成今日日志文件名
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);

    // 保存今日日期
    toDay_ = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(mtx_);
        buff_.RetrieveAll();
        if (fp_)
        {
            flush();
            fclose(fp_);
        }

        //-a表示写操作追加到文件末尾，文件不存在则创建
        fp_ = fopen(fileName, "a");
        // 如果创建失败则表示路径错误，需要先创建文件路径
        if (fp_ == nullptr)
        {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

void Log::AppendLogLevelTitle_(int level)
{
    switch (level)
    {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

void Log::write(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    // 获取当前的系统时间
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    // 调用localtime函数将秒数抓手你换为本地时间
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    // va_list获取函数的可变参数列表
    va_list vaList;

    // 如果当前日期和保存的日期对不上或者当前日志文件已写满
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0)))
    {
        std::unique_lock<std::mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 当前日期和保存的日期对不上
        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        // 当前日志文件已写满
        else
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }

        locker.lock();
        // 将缓冲区中的日志写入日志文件
        flush();
        // 关闭当前日志文件
        fclose(fp_);
        // 重新打开新的日志文件
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        std::unique_lock<std::mutex> locker(mtx_);
        // 写入日志数+1
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);

        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        // 获取可变参数
        va_start(vaList, format);
        // 向缓冲区答应规格化字符串
        int m = vsnprintf(buff_.BeginWrite(), buff_.WriteableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if (isAsync_ && deque_ && !deque_->full())
        {
            deque_->push_back(buff_.RetrieveAllToStr());
        }
        else
        {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

Log::~Log()
{
    // 如果异步并且异步线程未join
    if (writeThread_ && writeThread_->joinable())
    {
        while (!deque_->empty())
        {
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }
    if (fp_)
    {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

bool Log::IsOpen(){
    return isOpen_;
}