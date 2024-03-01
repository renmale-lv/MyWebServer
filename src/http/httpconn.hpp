#ifndef HTTP_CONN_HPP
#define HTTP_CONN_HPP

#include <arpa/inet.h>

#include "../log/log.hpp"
#include "../pool/sqlconnRAII.hpp"
#include "../buffer/buffer.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"

class HttpConn
{
public:
    HttpConn();
    ~HttpConn();
    void init(int sockFd, const sockaddr_in &addr);

    // 读取数据到读缓冲区，返回读取的字节数，即HTTP请求
    ssize_t read(int *saveErrno);

    // 将写缓冲区中的数据写入到连接，返回写入的字节数
    ssize_t write(int *saveErrno);

    // 关闭Http连接
    void Close();

    // 获取连接的文件描述符
    int GetFd() const;

    // 获取连接的端口号
    int GetPort() const;

    // 获取连接的IP地址
    const char *GetIP() const;

    // 获取连接的地址
    sockaddr_in GetAddr() const;

    // 处理HTTP请求
    bool process();

    // 获取待写入的字节数
    int ToWriteBytes();

    // 判断连接是否保持活动状态
    bool IsKeepAlive() const;

public:
    // 是否采用边沿触发模式
    static bool isET;

    // HTTP请求资源的根目录
    static const char *srcDir;

    // 静态变量，显示服务器有多少个http连接
    static std::atomic<int> userCount;

private:
    // HTTP连接的文件描述符
    int fd_;

    // 连接的地址
    struct sockaddr_in addr_;

    // 连接是否关闭
    bool isClose_;

    int iovCnt_;

    // 写缓冲区的 iovec 结构体数组
    struct iovec iov_[2];

    // 读缓冲区
    Buffer readBuff_;

    // 写缓冲区
    Buffer writeBuff_;

    // HTTP请求报文
    HttpRequest request_;

    // HTTP响应报文
    HttpResponse response_;
};

#endif