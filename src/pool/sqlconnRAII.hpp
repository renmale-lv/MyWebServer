/*
 * @Author: lvxr
 * @LastEditTime: 2024-03-24 21:57:05
 */
#ifndef SQLCONNRAII_HPP
#define SQLCONNRAII_HPP

#include <assert.h>
#include <mysql/mysql.h>

#include "sqlconnpool.hpp"

// RAII:资源在对象构造初始化 资源在对象析构时释放 类似unique_lock
class SqlConnRAII
{
public:
    // 两次取地址，注意
    SqlConnRAII(MYSQL **sql, SqlConnPool *connpool)
    {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~SqlConnRAII()
    {
        if (sql_)
            connpool_->FreeConn(sql_);
        // std::cout<<"free"<<std::endl;
    }

private:
    MYSQL *sql_;
    SqlConnPool *connpool_;
};

#endif