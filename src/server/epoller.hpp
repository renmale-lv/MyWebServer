#ifndef EPOLLER_HPP
#define EPOLLER_HPP

#include <vector>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>

// epoll操作类
class Epoller
{
public:
    // epoll_create 返回一个内核事件表，参数不起作用，给内核提示事件表需要多大
    explicit Epoller(int maxEvent = 1024)
        : epollFd_(epoll_create(512)), events_(maxEvent)
    {
        assert(epollFd_ >= 0 && events_.size() > 0);
    }

    ~Epoller()
    {
        close(epollFd_);
    }

    // 往事件表上注册要监听的文件描述符和要监听的事件
    bool AddFd(int fd, uint32_t events)
    {
        // 文件描述符必须大于等于0
        if (fd < 0)
            return false;
        // 初始化一个epoll_event
        epoll_event ev = {0};
        // 指定要监听的文件描述符
        ev.data.fd = fd;
        // 指定要监听的事件
        ev.events = events;
        return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    }

    // 修改fd上的注册事件
    bool ModFd(int fd, uint32_t events)
    {
        if (fd < 0)
            return false;
        epoll_event ev = {0};
        ev.data.fd = fd;
        ev.events = events;
        return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
    }

    // 删除fd上的注册事件
    bool DelFd(int fd)
    {
        if (fd < 0)
            return false;
        epoll_event ev = {0};
        return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
    }

    // 在一段超时时间timeoutMs内等待注册表上的事件，返回就绪的文件描述符数量
    int Wait(int timeoutMs)
    {
        return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
    }

    // 获取第i个就绪的文件描述符
    int GetEventFd(size_t i) const
    {
        assert(i < events_.size() && i >= 0);
        return events_[i].data.fd;
    }

    // 获取第i个就绪文件描述符上监听到的事件
    uint32_t GetEvents(size_t i) const
    {
        assert(i < events_.size() && i >= 0);
        return events_[i].events;
    }

private:
    // 时间表
    int epollFd_;
    // 就绪事件队列
    std::vector<struct epoll_event> events_;
};

#endif