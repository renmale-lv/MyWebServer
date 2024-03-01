#include "httpconn.hpp"

const char *HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn()
{
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

void HttpConn::Close()
{
    response_.UnmapFile();
    if (isClose_ == false)
    {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

HttpConn::~HttpConn() { Close(); };

void HttpConn::init(int fd, const sockaddr_in &addr)
{
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    // 清空写缓冲
    writeBuff_.RetrieveAll();
    // 清空读缓冲
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

ssize_t HttpConn::read(int *saveErrno)
{
    ssize_t len = -1;
    // 由于ET(边沿触发)模式只会通知一次事件，所以需要使用While循环将数据都读出
    do
    {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0)
            break;
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int *saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = writev(fd_, iov_, iovCnt_);
        // 写入失败
        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        // 全部数据已经被写入
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
            break;
        // 响应头数据已经写入，但消息体数据未完全写入
        else if (static_cast<size_t>(len) > iov_[0].iov_len)
        {
            // 偏移消息体数据
            iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len)
            {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        // 响应头数据未完全写入
        else
        {
            iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
        // 为什么是10240呢，刚好十倍的buff初始大小，效率？
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConn::process()
{
    request_.Init();
    // 读缓冲中没有数据，即没收到请求
    if (readBuff_.ReadableBytes() <= 0)
        return false;
    else if (request_.parse(readBuff_))
    {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else
        response_.Init(srcDir, request_.path(), false, 400);

    response_.MakeResponse(writeBuff_);
    // 响应头
    iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    // 文件（消息体）
    if (response_.FileLen() > 0 && response_.File())
    {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
    return true;
}

int HttpConn::ToWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }

bool HttpConn::IsKeepAlive() const { return request_.IsKeepAlive(); }

int HttpConn::GetFd() const { return fd_; };

struct sockaddr_in HttpConn::GetAddr() const { return addr_; }

const char *HttpConn::GetIP() const { return inet_ntoa(addr_.sin_addr); }

int HttpConn::GetPort() const { return addr_.sin_port; }