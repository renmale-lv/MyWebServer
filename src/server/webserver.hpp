#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.hpp"
#include "../log/log.hpp"
#include "../timer/heaptimer.hpp"
#include "../pool/sqlconnpool.hpp"
#include "../pool/threadpool.hpp"
#include "../pool/sqlconnRAII.hpp"
#include "../http/httpconn.hpp"

class WebServer
{
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char *sqlUser, const char *sqlPwd,
        const char *dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    ~WebServer();

    // 启动服务器
    void Start();

private:
    // 初始化服务器监听socket
    bool InitSocket_();

    // 根据指定的触发模式初始化事件模式
    void InitEventMode_(int trigMode);

    // 监听一个客户端连接
    void AddClient_(int fd, sockaddr_in addr);

    //
    void DealListen_();
    void DealWrite_(HttpConn *client);
    void DealRead_(HttpConn *client);

    void SendError_(int fd, const char *info);
    void ExtentTime_(HttpConn *client);
    void CloseConn_(HttpConn *client);

    void OnRead_(HttpConn *client);
    void OnWrite_(HttpConn *client);
    void OnProcess(HttpConn *client);

    static int SetFdNonblock(int fd);

private:
    static const int MAX_FD;
    int port_;
    bool openLinger_;
    int timeoutMS_; /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    char *srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};

#endif