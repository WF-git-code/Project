// 日志清理工具类
#ifndef LOG_CLEANER_HPP
#define LOG_CLEANER_HPP

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <cstring>

namespace logfile
{
    class LogCleaner
    {
    public:
        using CleanCallback = std::function<void(const std::string&)>;
        
        LogCleaner(const std::string& log_dir, int keep_days = 7)
            : log_dir_(log_dir), keep_days_(keep_days), running_(false)
        {
        }

        ~LogCleaner()
        {
            stop();
        }

        void setCallback(CleanCallback cb) {
            callback_ = cb;
        }

        void start()
        {
            if (running_) return;
            running_ = true;
            
            // 启动清理线程，每小时检查一次
            cleaner_thread_ = std::thread([this]() {
                while (running_)
                {
                    clean();
                    // 每小时检查一次
                    for (int i = 0; i < 3600 && running_; ++i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            });
        }

        void stop()
        {
            if (!running_) return;
            running_ = false;
            if (cleaner_thread_.joinable())
            {
                cleaner_thread_.join();
            }
        }

        void clean()
        {
            try
            {
                DIR* dir = opendir(log_dir_.c_str());
                if (!dir) return;

                std::time_t now = std::time(nullptr);
                std::time_t cutoff = now - (keep_days_ * 24 * 3600);

                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    std::string filename(entry->d_name);
                    
                    // 只处理.log文件
                    if (filename.find(".log") == std::string::npos) continue;

                    std::string filepath = log_dir_ + "/" + filename;
                    
                    // 获取文件修改时间
                    struct stat file_stat;
                    if (stat(filepath.c_str(), &file_stat) != 0) continue;

                    // 比较时间，删除超过7天的文件
                    if (file_stat.st_mtime < cutoff)
                    {
                        if (callback_) {
                            callback_(filepath);
                        }
                        remove(filepath.c_str());
                    }
                }

                closedir(dir);
            }
            catch (const std::exception& e)
            {
                // 忽略异常，继续运行
            }
        }

        // 立即清理，不启动线程
        void cleanNow()
        {
            clean();
        }

    private:
        std::string log_dir_;
        int keep_days_;
        bool running_;
        std::thread cleaner_thread_;
        CleanCallback callback_;
    };
}

#endif
