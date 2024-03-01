#ifndef BLOCKQUEUE_HPP
#define BLOCKQUEUE_HPP

#include <deque>
#include <mutex>
#include <condition_variable>

// 双端阻塞队列类
template <class T>
class BlockDeque
{
public:
    explicit BlockDeque(size_t MaxCapacity = 1000)
        : capacity_(MaxCapacity)
    {
        assert(MaxCapacity > 0);
        isClose_ = false;
    }

    // 关闭队列
    void Close()
    {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            deq_.clear();
            isClose_ = true;
        }
        condProducer_.notify_all();
        condConsumer_.notify_all();
    }

    ~BlockDeque() { Close(); }

    // 清空队列
    void clear()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
    }

    // 判断队列是否为空
    bool empty()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return deq_.empty();
    }

    // 判断队列是否满了
    bool full()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return deq_.size() >= capacity_;
    }

    // 返回容器内元素数量
    size_t size()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return deq_.size();
    }

    // 返回容器容量
    size_t capacity()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return capacity_;
    }

    // 返回队列首部元素
    T front()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return deq_.front();
    }

    // 返回队列尾部元素
    T back()
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return deq_.back();
    }

    // 在尾部插入一个元素
    void push_back(const T &item)
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while (deq_.size() >= capacity_)
        {
            // 容器满了则阻塞等待
            condProducer_.wait(locker);
        }
        deq_.push_back(item);
        // 通知一个客户
        condConsumer_.notify_one();
    }

    // 在首部插入一个元素
    void push_front(const T &item)
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while (deq_.size() >= capacity_)
        {
            condProducer_.wait(locker);
        }
        deq_.push_front(item);
        condConsumer_.notify_one();
    }

    // 从首部弹出一个元素
    bool pop(T &item)
    {
        std::unique_lock<std::mutex> locker(mtx_);
        // 如果容器为空则阻塞等待
        while (deq_.empty())
        {
            condConsumer_.wait(locker);
            if (isClose_)
            {
                return false;
            }
        }
        item = deq_.front();
        deq_.pop_front();
        // 通知生产者有空闲空间
        condProducer_.notify_one();
        return true;
    }

    // 在超时时间内弹出一个元素
    bool pop(T &item, int timeout)
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while (deq_.empty())
        {
            if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout)
            {
                return false;
            }
            if (isClose_)
            {
                return false;
            }
        }
        item = deq_.front();
        deq_.pop_front();
        condProducer_.notify_one();
        return true;
    }

    // 通知一个客户,即有元素可以弹出
    void flush()
    {
        condConsumer_.notify_one();
    }

private:
    // 底层队列
    std::deque<T> deq_;
    // 队列容量
    size_t capacity_;
    // 互斥量，用于保护deq_
    std::mutex mtx_;
    bool isClose_;
    // 客户条件变量
    std::condition_variable condConsumer_;
    // 生存者条件变量
    std::condition_variable condProducer_;
};

#endif