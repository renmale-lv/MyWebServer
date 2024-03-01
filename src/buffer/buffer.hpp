#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <iostream>
#include <vector>
#include <atomic>
#include <cstring>
#include <assert.h>
#include <unistd.h>
#include <sys/uio.h>

/**
 * 应用层缓冲区的设计
 * https://blog.csdn.net/daaikuaichuan/article/details/88814044
 */

// 缓冲区类
class Buffer
{
public:
    Buffer(int initBuffSize = 1024)
        : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

    // 使用默认析构函数
    ~Buffer() = default;

public:
    // 返回可以写多少字节数
    size_t WriteableBytes() const { return buffer_.size() - writePos_; }

    // 返回可以读多少字节数
    size_t ReadableBytes() const { return writePos_ - readPos_; }

    // 预置数据，0~readPos部分的数据
    size_t PrependableBytes() const { return readPos_; }

    // 返回读部分的起始地址
    const char *Peek() const { return BeginPtr_() + readPos_; }

    // 确认读到len长度的可读数据，将readPos向后推
    void Retrieve(size_t len)
    {
        assert(len <= ReadableBytes());
        readPos_ += len;
    }

    // 确认读直到end数据
    void RetrieveUntil(const char *end)
    {
        assert(Peek() <= end);
        Retrieve(end - Peek());
    }

    // 确认读到全部数据
    void RetrieveAll()
    {
        // 直接清空整个buffer，因为要读的是readPos~writePos之间的数据，而writePos后的数据是未写的，也是空的，故直接清空buffer就行
        bzero(&buffer_[0], buffer_.size());
        readPos_ = 0;
        writePos_ = 0;
    }

    // 返回全部可读数据并清空缓冲区
    std::string RetrieveAllToStr()
    {
        std::string str(Peek(), ReadableBytes());
        RetrieveAll();
        return str;
    }

    // 返回可写部分起始地址
    char *BeginWrite() { return BeginPtr_() + writePos_; }
    const char *BeginWriteConst() const { return BeginPtr_() + writePos_; }

    // 确认写len长度
    void HasWritten(size_t len) { writePos_ += len; }

    // 确认是否能够写len长度的数据，如果不行则对buffer进行扩充
    void EnsureWriteable(size_t len)
    {
        if (WriteableBytes() < len)
        {
            MakeSpace_(len);
        }
        assert(WriteableBytes() >= len);
    }

    // 往缓冲区填入数据
    void Append(const char *str, size_t len)
    {
        assert(str);
        EnsureWriteable(len);
        std::copy(str, str + len, BeginWrite());
        HasWritten(len);
    }
    void Append(const std::string &str) { Append(str.data(), str.length()); }
    void Append(const void *data, size_t len)
    {
        assert(data);
        Append(static_cast<const char *>(data), len);
    }
    void Append(const Buffer &buff) { Append(buff.Peek(), buff.ReadableBytes()); }

    // 从fd中读出数据并写入缓冲区中
    ssize_t ReadFd(int fd, int *saveErrno)
    {
        char buff[65535];
        struct iovec iov[2];
        const size_t writeable = WriteableBytes();

        /**
         * 使用readv函数进行多块读
         * iov[0]将读出的数据先写入缓冲区
         * iov[1]用于承载缓冲区存不下的数据
         */
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = writeable;
        iov[1].iov_base = buff;
        iov[1].iov_len = sizeof(buff);

        const ssize_t len = readv(fd, iov, 2);
        if (len < 0)
        {
            // 出错
            *saveErrno = errno;
        }
        else if (static_cast<size_t>(len) <= writeable)
        {
            // 缓冲区可以承载全部读出的数据
            writePos_ += len;
        }
        else
        {
            // 缓冲区不可以承载全部读出的数据，将溢出的数据buff写入缓冲区
            writePos_ = buffer_.size();
            Append(buff, len - writeable);
        }
        return len;
    }

    // 将缓冲区中可读的数据写入fd中
    ssize_t WriteFd(int fd, int *saveErrno)
    {
        ssize_t readSize = ReadableBytes();
        ssize_t len = write(fd, Peek(), readSize);
        if (len < 0)
        {
            *saveErrno = errno;
            return len;
        }
        readPos_ += len;
        return len;
    }

private:
    /**
     * 返回缓冲区起始地址
     * buffer_.begin()返回一个迭代器，指向容器的第一个元素
     * *buffer_.begin()解引用该迭代器，得到第一个元素的值
     * &*buffer_.begin()取得该值的地址，即获取第一个元素的指针
     */
    char *BeginPtr_() { return &*buffer_.begin(); }
    const char *BeginPtr_() const { return &*buffer_.begin(); }

    // 扩充buffer或者重新整理buffer，将已读数据所占的空间迁移到buffer末尾
    void MakeSpace_(size_t len)
    {
        // 如果可写空间+已读空间不能满足要写的长度，则对buffer进行扩充
        if (WriteableBytes() + PrependableBytes() < len)
        {
            buffer_.resize(writePos_ + len + 1);
        }
        // 若满足，则迁移buffer
        else
        {
            size_t readable = ReadableBytes();
            std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
            readPos_ = 0;
            writePos_ = readable + readPos_;
            assert(readable == ReadableBytes());
        }
    }

private:
    // 缓冲区
    std::vector<char> buffer_;
    // 缓冲区读、写位置，使用原子变量保证多线程间同步
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif