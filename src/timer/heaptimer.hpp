#ifndef HEAP_TIMER_HPP
#define HEAP_TIMER_HPP

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>

// 函数对象类型，用于表示超时回调函数
typedef std::function<void()> TimeoutCallBack;
// 用于测量时间间隔
typedef std::chrono::high_resolution_clock Clock;
// 表示以毫秒为单位的时间间隔
typedef std::chrono::milliseconds MS;
// 表示一个时间点
typedef Clock::time_point TimeStamp;

// 定时器节点结构体
struct TimerNode
{
    // 定时器节点唯一标识
    int id;
    // 定时器节点的过期时间点
    TimeStamp expires;
    // 定时器节点的超时回调函数
    TimeoutCallBack cb;
    bool operator<(const TimerNode &t)
    {
        return expires < t.expires;
    }
};

// 定时器堆
class HeapTimer
{
public:
    HeapTimer()
    {
        // 将heap_容量改为64
        heap_.reserve(64);
    }

    ~HeapTimer() { clear(); }

    // 更新指定id的定时器的超时时间
    void adjust(int id, int newExpires)
    {
        assert(!heap_.empty() && ref_.count(id) > 0);
        heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
        siftdown_(ref_[id], heap_.size());
    }

    // 添加定时器，如果id已存在，则修改原定时器
    void add(int id, int timeOut, const TimeoutCallBack &cb)
    {
        assert(id >= 0);
        size_t i;
        if (ref_.count(id) == 0)
        {
            // 新节点，堆尾插入，调整堆
            i = heap_.size();
            ref_[id] = i;
            heap_.push_back({id, Clock::now() + MS(timeOut), cb});
            siftup_(i);
        }
        else
        {
            // 已有结点：调整堆
            i = ref_[id];
            heap_[i].expires = Clock::now() + MS(timeOut);
            heap_[i].cb = cb;
            if (!siftdown_(i, heap_.size()))
            {
                siftup_(i);
            }
        }
    }

    // 删除指定id结点，并触发回调函数，即立刻触发定时器
    void doWork(int id)
    {
        // 如果定时器不存在
        if (heap_.empty() || ref_.count(id) == 0)
        {
            return;
        }
        size_t i = ref_[id];
        TimerNode node = heap_[i];
        node.cb();
        del_(i);
    }

    // 清空定时器
    void clear()
    {
        ref_.clear();
        heap_.clear();
    }

    // 清除超时节点并执行对应的回调函数
    void tick()
    {
        if (heap_.empty())
            return;
        while (!heap_.empty())
        {
            TimerNode node = heap_.front();
            // 堆顶元素未超时
            if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
                break;
            // 执行超时定时器的回调函数
            node.cb();
            pop();
        }
    }

    // 弹出堆顶元素
    void pop()
    {
        assert(!heap_.empty());
        del_(0);
    }

    // 返回还有多少ms触发下一个定时器,没有定时器可以触发则返回-1；
    int GetNextTick()
    {
        tick();
        size_t res = -1;
        if (!heap_.empty())
        {
            res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
            if (res < 0)
                res = 0;
        }
        return res;
    }

private:
    // 删除指定定时器节点
    void del_(size_t index)
    {
        assert(!heap_.empty() && index >= 0 && index < heap_.size());
        // 将要删除的结点换到队尾，然后调整堆
        size_t i = index;
        size_t n = heap_.size() - 1;
        assert(i <= n);
        if (i < n)
        {
            SwapNode_(i, n);
            if (!siftdown_(i, n))
            {
                siftup_(i);
            }
        }
        // 删除队尾元素
        ref_.erase(heap_.back().id);
        heap_.pop_back();
    }

    // 小根堆-向上调整节点
    void siftup_(size_t i)
    {
        assert(i >= 0 && i < heap_.size());
        size_t j = (i - 1) / 2;
        while (j >= 0)
        {
            if (heap_[j] < heap_[i])
                break;
            SwapNode_(i, j);
            i = j;
            j = (i - 1) / 2;
        }
    }

    // 小根堆-向下调整节点
    bool siftdown_(size_t index, size_t n)
    {
        assert(index >= 0 && index < heap_.size());
        assert(n >= 0 && n <= heap_.size());
        size_t i = index;
        size_t j = i * 2 + 1;
        while (j < n)
        {
            if (j + 1 < n && heap_[j + 1] < heap_[j])
                j++;
            if (heap_[i] < heap_[j])
                break;
            SwapNode_(i, j);
            i = j;
            j = i * 2 + 1;
        }
        return i > index;
    }

    // 小根堆-交换两个节点
    void SwapNode_(size_t i, size_t j)
    {
        assert(i >= 0 && i < heap_.size());
        assert(j >= 0 && j < heap_.size());
        std::swap(heap_[i], heap_[j]);
        ref_[heap_[i].id] = i;
        ref_[heap_[j].id] = j;
    }

    // 定时器数组
    std::vector<TimerNode> heap_;

    // 小根堆
    std::unordered_map<int, size_t> ref_;
};

#endif