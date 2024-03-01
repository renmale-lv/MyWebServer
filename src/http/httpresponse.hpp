#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_map>

#include "../buffer/buffer.hpp"
#include "../log/log.hpp"

// HTTP应答类
class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    // 初始化
    void Init(const std::string &srcDir, std::string &path, bool isKeepAlive = false, int code = -1);

    // 生成 HTTP 响应，将响应内容写入到给定的 Buffer 对象中
    void MakeResponse(Buffer &buff);

    // 取消映射文件，用于释放内存映射的文件
    void UnmapFile();

    // 获取内存映射文件的指针
    char *File();

    // 获取内存映射文件的长度
    size_t FileLen() const;

    // 生成错误响应内容，并将其写入到给定的 Buffer 对象中
    void ErrorContent(Buffer &buff, std::string message);

    // 获取响应状态码
    int Code() const;

private:
    // 添加响应状态行到 Buffer 对象中
    void AddStateLine_(Buffer &buff);

    // 添加响应头部到 Buffer 对象中
    void AddHeader_(Buffer &buff);

    // 添加响应内容到 Buffer 对象中
    void AddContent_(Buffer &buff);

    // 生成错误页面的 HTML 内容
    void ErrorHtml_();

    // 获取文件类型的对应的MIME类型
    std::string GetFileType_();

private:
    // 响应状态码
    int code_;

    // 是否保持连接
    bool isKeepAlive_;

    // 请求路径
    std::string path_;

    // 响应的源目录
    std::string srcDir_;

    // 内存映射文件的指针
    char *mmFile_;

    // 内存映射文件的状态信息
    struct stat mmFileStat_;

    // 文件扩展名和 MIME 类型的映射表
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;

    // 响应状态码喝对应描述的映射表
    static const std::unordered_map<int, std::string> CODE_STATUS;

    // 响应状态码喝对应错误页面路径的映射表
    static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif