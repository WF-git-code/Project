#include "AsynLogging.hpp"

namespace logfile {

// 单个缓冲区的最大长度（4MB，比之前的4KB大1000倍，减少锁争用）
const size_t BufMaxLen = 1024 * 1024 * 4;
const size_t BufQueueSize = 16;

void AsynLogging::workThreadFunc() {
    std::vector<std::string> buffersToWrite;
    latch_.countDown();
    
    while (running_) {
        {
            std::unique_lock<std::mutex> locker(mutex_);
            while (buffers_.empty() && running_) {
                cond_.wait_for(locker, std::chrono::milliseconds(200));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_.reserve(BufMaxLen);
            buffersToWrite.swap(buffers_);
            buffers_.reserve(BufQueueSize);
        }
        
        if (buffersToWrite.size() > 25) {
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }
        
        for (auto &buf : buffersToWrite) {
            output_.append(buf);
        }
        buffersToWrite.clear();
    }
    
    {
        std::unique_lock<std::mutex> locker(mutex_);
        if (!currentBuffer_.empty()) {
            buffers_.push_back(std::move(currentBuffer_));
        }
        buffersToWrite.swap(buffers_);
    }
    
    for (auto &buf : buffersToWrite) {
        if (!buf.empty()) {
            output_.append(buf);
        }
    }
    output_.flush();
}

AsynLogging::AsynLogging(const std::string &bname,
                         size_t rollSize,
                         int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(bname),
      rollSize_(rollSize),
      latch_(1),
      output_{bname, rollSize} {
    currentBuffer_.reserve(BufMaxLen);
    buffers_.reserve(BufQueueSize);
}

AsynLogging::~AsynLogging() {
    if (running_) {
        stop();
    }
}

void AsynLogging::append(const std::string &msg) {
    append(msg.c_str(), msg.size());
}

void AsynLogging::append(const char *msg, const size_t len) {
    std::unique_lock<std::mutex> locker(mutex_);
    
    if (currentBuffer_.size() >= BufMaxLen ||
        currentBuffer_.capacity() - currentBuffer_.size() < len) {
        buffers_.push_back(std::move(currentBuffer_));
        currentBuffer_.reserve(BufMaxLen);
    }
    
    currentBuffer_.append(msg, len);
    cond_.notify_one();
}

void AsynLogging::start() {
    running_ = true;
    pthread_.reset(new std::thread(&AsynLogging::workThreadFunc, this));
    latch_.wait();
}

void AsynLogging::stop() {
    running_ = false;
    cond_.notify_all();
    if (pthread_ && pthread_->joinable()) {
        pthread_->join();
    }
}

void AsynLogging::flush() {
    std::vector<std::string> bufferToWrite;
    {
        std::unique_lock<std::mutex> locker(mutex_);
        if (!currentBuffer_.empty()) {
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_.reserve(BufMaxLen);
        }
        bufferToWrite.swap(buffers_);
    }
    
    for (auto &buf : bufferToWrite) {
        output_.append(buf);
    }
    output_.flush();
    
    std::lock_guard<std::mutex> locker(mutex_);
    buffers_.reserve(BufQueueSize);
}

} // namespace logfile
