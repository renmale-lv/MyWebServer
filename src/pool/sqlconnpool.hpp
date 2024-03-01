#ifndef SQLCONNPOOL_HPP
#define SQLCONNPOOL_HPP

#include <mysql/mysql.h>
#include <string>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <assert.h>

#include "../log/log.hpp"

// 单例类，数据库连接池
class SqlConnPool
{
public:
    // 单例
    static SqlConnPool *Instance();

    // 获取一个数据库连接
    MYSQL *GetConn();

    // 释放指定数据库连接
    void FreeConn(MYSQL *conn);

    // 获取可用（空闲）数据库连接数量
    int GetFreeConnCount();

    // 初始化连接池
    void Init(const char *host, int port, const char *user, const char *pwd, const char *dbName, int connSize);

    // 关闭连接池
    void ClosePool();

private:
    SqlConnPool(){}

    ~SqlConnPool();

    // 最大连接数量
    int MAX_CONN_;

    // 连接队列
    std::queue<MYSQL *> connQue_;

    // 连接队列互斥量
    std::mutex mtx_;

    // 连接数量信号量
    sem_t semId_;
};

#endif