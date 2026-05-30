#include <stdio.h>
#include "Logger.hpp"

namespace logfile
{
    logfile::LOG_LEVEL InitLogLevel()
    {
        if (::getenv("LOGFILE::LOG_TRACE"))
        {
            return logfile::LOG_LEVEL::TRACE;
        }
        else if (::getenv("LOGFILE::LOG_DEBUG"))
        {
            return logfile::LOG_LEVEL::DEBUG;
        }
        else
        {
            return logfile::LOG_LEVEL::INFO;
        }
    }

    void defaultOutput(const std::string &mes)
    {
        size_t n = fwrite(mes.c_str(), sizeof(char), mes.size(), stdout);
        (void)n;
    }

    void defaultFlush()
    {
        fflush(stdout);
    }

    Logger::OutputFunc Logger::s_output_ = defaultOutput;
    Logger::FlushFunc Logger::s_flush_ = defaultFlush;

    void Logger::setOutput(OutputFunc func)
    {
        s_output_ = func;
    }

    void Logger::setFlush(FlushFunc func)
    {
        s_flush_ = func;
    }

    Logger::Logger(const logfile::LOG_LEVEL &level,
                   const std::string &filename,
                   const std::string &funcname,
                   const int line)
        : impl_(level, filename, funcname, line)
    {
    }

    Logger::~Logger()
    {
        impl_ << "\n";
        std::string log_str = impl_.toString();
        s_output_(log_str);
        s_flush_();
        
        if (impl_.getLogLevel() == logfile::LOG_LEVEL::FATAL)
        {
            fprintf(stderr, "Process exit \n");
            exit(EXIT_FAILURE);
        }
    }

    logfile::LogMessage &Logger::stream()
    {
        return impl_;
    }

    logfile::LOG_LEVEL Logger::s_level_ = InitLogLevel();

    logfile::LOG_LEVEL Logger::getLogLevel()
    {
        return s_level_;
    }

    void Logger::setLogLevel(const logfile::LOG_LEVEL &level)
    {
        s_level_ = level;
    }

} // namespace logfile
