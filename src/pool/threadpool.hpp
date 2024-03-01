#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <thread>

// 线程池
class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount = 8)
        : pool_(std::make_shared<Pool>())
    {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; ++i)
        {
            // 用传值捕获共享指针pool_
            std::thread(
                [pool = pool_]
                {
                    // 创建时自动对互斥量进行上锁
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while (true)
                    {
                        // 如果任务队列非空
                        if (!pool->tasks.empty())
                        {
                            // 移动语义，比copy高效
                            auto task = std::move(pool->tasks.front());
                            pool->tasks.pop();
                            // 解锁，使其他线程可以访问任务队列
                            locker.unlock();
                            task();
                            // 任务执行完毕，上锁
                            locker.lock();
                        }
                        else if (pool->isClosed)
                        {
                            // 线程池关闭则退出线程
                            break;
                        }
                        else
                        {
                            /**
                             * 任务队列为空时
                             * 使用条件变量先释放locker锁，允许其他线程访问任务队列
                             * 再让该线程等待，直到被通知，这时再重新尝试去获取锁
                             */
                            pool->cond.wait(locker);
                        }
                    }
                })
                .detach();
        }
    }

    // 使用默认构造函数和移动构造函数
    ThreadPool() = default;
    ThreadPool(ThreadPool &&) = default;

    ~ThreadPool()
    {
        // 强制类型转换，判断pool_是否存在
        if (static_cast<bool>(pool_))
        {
            {
                /**lock_guard：构造时获得锁，析构时释放锁
                 * 相比于unique_lock无法手动获得锁和释放锁
                 *用{}包裹形成局部作用域，退出{}时自动析构锁
                 */
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            //通知所有线程，使线程退出
            pool_->cond.notify_all();
        }
    }

    template <class T>
    void AddTask(T &&task)
    {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            /**
             * emplace()成员函数再容器末尾就地构造一个新的对象
             * emplace效率高于insert
             * std::forward 完美转发
            */
            pool_->tasks.emplace(std::forward<T>(task));
        }
        pool_->cond.notify_one();
    }

private:
    struct Pool
    {
        std::mutex mtx;
        // 条件变量
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    // 共享指针
    std::shared_ptr<Pool> pool_;
};

#endif