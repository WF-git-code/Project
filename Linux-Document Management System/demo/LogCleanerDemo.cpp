// 日志清理演示程序
// 功能：创建测试日志文件，演示定时清理7天前的日志

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <sys/stat.h>
#include <utime.h>
#include <thread>
#include <chrono>
#include "../server/LogFile/include/LogCleaner.hpp"

void createTestFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    file << content << std::endl;
    file.close();
}

void setFileTime(const std::string& path, time_t days_ago)
{
    // 设置文件时间为N天前
    time_t old_time = time(nullptr) - (days_ago * 24 * 3600);
    struct utimbuf new_times;
    new_times.actime = old_time;
    new_times.modtime = old_time;
    utime(path.c_str(), &new_times);
}

int main()
{
    std::cout << "========== 日志清理演示程序 ==========" << std::endl;
    
    // 创建测试目录
    std::string log_dir = "./test_logs";
    system(("mkdir -p " + log_dir).c_str());
    
    // 创建测试日志文件
    std::cout << "\n1. 创建测试日志文件..." << std::endl;
    
    std::string today_file = log_dir + "/server_today.log";
    createTestFile(today_file, "今天的日志 - 2026-01-29");
    std::cout << "  创建: server_today.log (今天)" << std::endl;
    
    std::string yesterday_file = log_dir + "/server_yesterday.log";
    createTestFile(yesterday_file, "昨天的日志 - 2026-01-28");
    setFileTime(yesterday_file, 1);  // 1天前
    std::cout << "  创建: server_yesterday.log (1天前)" << std::endl;
    
    std::string old3_file = log_dir + "/server_3days.log";
    createTestFile(old3_file, "3天前的日志 - 2026-01-26");
    setFileTime(old3_file, 3);  // 3天前
    std::cout << "  创建: server_3days.log (3天前)" << std::endl;
    
    std::string old8_file = log_dir + "/server_8days.log";
    createTestFile(old8_file, "8天前的日志 - 应该被删除");
    setFileTime(old8_file, 8);  // 8天前
    std::cout << "  创建: server_8days.log (8天前)" << std::endl;
    
    std::string old10_file = log_dir + "/server_10days.log";
    createTestFile(old10_file, "10天前的日志 - 应该被删除");
    setFileTime(old10_file, 10);  // 10天前
    std::cout << "  创建: server_10days.log (10天前)" << std::endl;
    
    std::cout << "\n2. 验证文件创建成功..." << std::endl;
    system(("ls -lh " + log_dir + "/*.log 2>/dev/null | awk '{print $6, $7, $8, $9}'").c_str());
    
    // 显示文件详情
    std::cout << "\n3. 各文件详情:" << std::endl;
    for (const auto& name : {"today", "yesterday", "3days", "8days", "10days"}) {
        std::string filepath = log_dir + "/server_" + name + ".log";
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0) {
            char date[100];
            struct tm* tm_info = localtime(&st.st_mtime);
            strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", tm_info);
            std::cout << "  " << name << ": " << date;
            
            // 判断是否应该被删除
            time_t now = time(nullptr);
            int age_days = (now - st.st_mtime) / (24 * 3600);
            if (age_days > 7) {
                std::cout << " [将删除]" << std::endl;
            } else {
                std::cout << " [保留]" << std::endl;
            }
        }
    }
    
    // 执行清理
    std::cout << "\n4. 执行日志清理（保留7天）..." << std::endl;
    logfile::LogCleaner cleaner(log_dir, 7);
    
    cleaner.setCallback([](const std::string& file) {
        std::cout << "  ✓ 删除: " << file << std::endl;
    });
    
    cleaner.cleanNow();
    
    // 显示清理后的结果
    std::cout << "\n5. 清理后的日志文件:" << std::endl;
    system(("ls -lh " + log_dir + "/*.log 2>/dev/null | awk '{print $6, $7, $8, $9}'").c_str());
    
    std::cout << "\n6. 演示定时清理（3秒）..." << std::endl;
    std::cout << "  启动定时清理线程（每小时检查一次）" << std::endl;
    
    cleaner.start();
    std::cout << "  等待3秒..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "  停止清理线程" << std::endl;
    cleaner.stop();
    
    // 最终状态
    std::cout << "\n7. 最终文件状态:" << std::endl;
    int count = system(("ls " + log_dir + "/*.log 2>/dev/null | wc -l").c_str());
    if (count == 0) {
        std::cout << "  所有日志文件已清理完毕" << std::endl;
    } else {
        system(("ls -lh " + log_dir + "/*.log 2>/dev/null | awk '{print \"  \"$6, $7, $8, $9}'").c_str());
    }
    
    // 清理测试环境
    std::cout << "\n8. 清理测试环境..." << std::endl;
    system(("rm -rf " + log_dir).c_str());
    std::cout << "  测试目录已删除" << std::endl;
    
    std::cout << "\n========== 演示完成 ==========" << std::endl;
    std::cout << "\n功能说明:" << std::endl;
    std::cout << "  - 自动删除7天前的日志文件" << std::endl;
    std::cout << "  - 每小时自动检查一次" << std::endl;
    std::cout << "  - 只删除 .log 后缀的文件" << std::endl;
    std::cout << "  - 支持回调函数记录删除操作" << std::endl;
    std::cout << "\n使用方法:" << std::endl;
    std::cout << "  集成到LoggerManager中，程序启动时自动运行" << std::endl;
    std::cout << "\n更多信息请查看: docs/日志清理使用说明.md" << std::endl;
    
    return 0;
}
