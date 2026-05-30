// 日志管理器单例类
#ifndef LOGGER_MANAGER_HPP
#define LOGGER_MANAGER_HPP

#include "AsynLogging.hpp"
#include "Logger.hpp"
#include "LogCleaner.hpp"
#include <string>
#include <memory>

namespace logfile
{
    class LoggerManager
    {
    public:
        static LoggerManager& getInstance() {
            static LoggerManager instance;
            return instance;
        }

        void init(const std::string& basename, int keep_days = 7, const std::string& log_dir = "./", bool enable_clean = true) {
            basename_ = basename;
            keep_days_ = keep_days;
            log_dir_ = log_dir;
            enable_clean_ = enable_clean;
            
            // 创建新的LogCleaner实例
            cleaner_ = std::make_unique<LogCleaner>(log_dir, keep_days);
        }

        void start() {
            // 必须先设置输出回调，再启动日志系统
            Logger::setOutput([this](const std::string &msg) {
                g_log.append(msg);
            });
            Logger::setFlush([this]() {
                g_log.flush();
            });
            
            g_log.start();
            
            // 根据配置决定是否启动日志清理器
            if (enable_clean_ && cleaner_) {
                LOG_INFO << "启动日志清理器，保留 " << keep_days_ << " 天日志";
                cleaner_->setCallback([](const std::string& file) {
                    LOG_INFO << "删除过期日志: " << file;
                });
                cleaner_->start();
            }
        }

        void stop() {
            if (enable_clean_ && cleaner_) {
                cleaner_->stop();
            }
            g_log.stop();
        }

        void setOutput() {
            Logger::setOutput([this](const std::string &msg) {
                g_log.append(msg);
            });
        }

        void setFlush() {
            Logger::setFlush([this]() {
                g_log.flush();
            });
        }

        void setLevel(LOG_LEVEL level) {
            Logger::setLogLevel(level);
        }

        LOG_LEVEL getLevel() {
            return Logger::getLogLevel();
        }

        // 手动触发日志清理
        void cleanLogs() {
            if (enable_clean_ && cleaner_) {
                cleaner_->cleanNow();
            }
        }

    private:
        LoggerManager() : enable_clean_(false) {
            // 默认初始化
            cleaner_ = std::make_unique<LogCleaner>(".", 7);
        }
        ~LoggerManager() = default;
        LoggerManager(const LoggerManager&) = delete;
        LoggerManager& operator=(const LoggerManager&) = delete;

        std::string basename_;
        int keep_days_;
        std::string log_dir_;
        bool enable_clean_;
        AsynLogging g_log{"server"};
        std::unique_ptr<LogCleaner> cleaner_;
    };
}

#endif
