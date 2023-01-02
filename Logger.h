#ifndef LOGGER_BY_YQ
#define LOGGER_BY_YQ

#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <cstdarg>

// 获取时间字符串
inline std::string GetDateTimeStr()
{
    time_t t = time(NULL);
    tm *ts = localtime(&t);
    char buf[24] = { 0 };
    strftime(buf, 24, "%Y-%m-%d %H:%M:%S", ts);
    return std::string(buf);
}

// Logger
class Logger
{
public:
    // 限制仅输出>=日志等级的日志，避免刷屏
    enum class LogLevel { DEBUG, INFO, WARN, ERROR, FATAL, NONE };

private:
    std::string prefix;
    LogLevel logLevel;

public:
    Logger(LogLevel logLevel = LogLevel::INFO, std::string prefix = "")
    {
        this->prefix = prefix;
        this->logLevel = logLevel;
    }

    // 修改log前缀
    void setPrefix(std::string prefix)
    {
        this->prefix = prefix;
    }

    // 修改日志等级
    void setLogLevel(LogLevel level)
    {
        this->logLevel = level;
    }

    // debug
    void debug(const char* format, ...)
    {
        if(logLevel > LogLevel::DEBUG)
            return;
        std::string dateTime = GetDateTimeStr();

        // stdout
        printf("[%s DEBUG]", dateTime.c_str());
        if(!prefix.empty())
            printf(" [%s] ", prefix.c_str());
        else
            printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }

    // info
    void info(const char* format, ...)
    {
        if(logLevel > LogLevel::INFO)
            return;
        std::string dateTime = GetDateTimeStr();

        // stdout
        printf("[%s INFO]", dateTime.c_str());
        if(!prefix.empty())
            printf(" [%s] ", prefix.c_str());
        else
            printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }

    // warning
    void warn(const char* format, ...)
    {
        if(logLevel > LogLevel::WARN)
            return;
        std::string dateTime = GetDateTimeStr();

        // stdout
        printf("[%s WARN]", dateTime.c_str());
        if(!prefix.empty())
            printf(" [%s] ", prefix.c_str());
        else
            printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }

    // error
    void error(const char* format, ...)
    {
        if(logLevel > LogLevel::ERROR)
            return;
        std::string dateTime = GetDateTimeStr();

        // stdout
        printf("[%s ERROR]", dateTime.c_str());
        if(!prefix.empty())
            printf(" [%s] ", prefix.c_str());
        else
            printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }

    // fatal
    void fatal(const char* format, ...)
    {
        if(logLevel > LogLevel::FATAL)
            return;
        std::string dateTime = GetDateTimeStr();

        // stdout
        printf("[%s FATAL]", dateTime.c_str());
        if(!prefix.empty())
            printf(" [%s] ", prefix.c_str());
        else
            printf(" ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
        fflush(stdout);
    }
};

#endif