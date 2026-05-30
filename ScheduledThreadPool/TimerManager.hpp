#ifndef TIMERMAP3_HPP
#define TIMERMAP3_HPP
#include "Timer.hpp"
#include <sys/epoll.h>
#include <map>
#include <unordered_map>
#include <thread>
#include <vector>
#include<memory>
#include"CachedThreadPools.hpp"
using namespace std;
class TimerManager
{
private:
    int m_epollfd;
    std::vector<epoll_event> m_events;
    std::unordered_map<int,std::shared_ptr< Timer>> m_timers;
    bool m_stop; // false;
    static const int eventsize = 16;
    std::thread m_workerThread;
    CachedThreadPool m_pool;
    void remove_timer(int fd)
    {
        if (m_timers.find(fd) == m_timers.end())
            return;
        epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd, nullptr);
        m_timers.erase(fd);
    }

public:
    TimerManager():m_epollfd(-1),m_stop(false),m_pool(1000,4)
    {
        m_events.resize(eventsize);
    }
    ~TimerManager()
    {
        set_stop();
    }
    TimerManager(const TimerManager &) = delete;
    TimerManager &operator=(const TimerManager &) = delete;
    bool init(int timeout)
    {
        bool ret = false;
        m_epollfd = epoll_create1(EPOLL_CLOEXEC);
        if (m_epollfd > 0)
        {
            try
            {
                m_workerThread = std::thread(&TimerManager::loop, this, timeout);
                ret = true;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
                close(m_epollfd);
                m_epollfd = -1;
            }
        }
        return ret;
    }
    bool add_timer(std::shared_ptr< Timer>&tv)
    {
        struct epoll_event ev = {0};
        ev.data.fd = tv->get_fd();
        ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, tv->get_fd(), &ev) < 0)
        {
            fprintf(stderr, "epoll_ctl_add failer: %s\n", strerror(errno));
            return false;
        }
        m_timers[tv->get_fd()] = tv;
        return true;
    }
    void loop(int timeout)
    {
        while (!m_stop)
        {
            int n = epoll_wait(m_epollfd, m_events.data(), m_events.size(), timeout);
            cout << "epoll_wait n: " << n << endl;
            for (int i = 0; i < n; ++i)
            {
                int fd = m_events[i].data.fd;
                auto it = m_timers.find(fd);
                if (it != m_timers.end())
                {
                    auto &tv = it->second;
                    //tv->handle_event();
                    m_pool.excute([tv]{tv->handle_event();});
                    //m_pool.excute(std::bind(&Timer::handle_event,tv));
                    if(tv->IsOnce()){
                        remove_timer(fd);
                    }
                }
            }
            if (n >= m_events.size())
            {
                m_events.resize(m_events.size() * 2);
            }
        }
    }
    void set_stop()
    {
        m_stop = true;
        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }
        close(m_epollfd);
        m_epollfd = -1;
    }
};

#endif