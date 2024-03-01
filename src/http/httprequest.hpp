#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <mysql/mysql.h>

#include "../buffer/buffer.hpp"
#include "../log/log.hpp"
#include "../pool/sqlconnpool.hpp"
#include "../pool/sqlconnRAII.hpp"

// HTTP请求类
class HttpRequest
{
public:
    // HTTP解析状态的枚举
    enum PARSE_STATE
    {
        // 请求行
        REQUEST_LINE,
        // 头部
        HEADERS,
        // 消息体
        BODY,
        // 解析完成
        FINISH,
    };

    // HTTP请求状态码枚举
    enum HTTP_CODE
    {
        // 无请求
        NO_REQUEST = 0,
        // GET请求
        GET_REQUEST,
        // 错误请求
        BAD_REQUEST,
        // 无资源
        NO_RESOURSE,
        // 禁止请求
        FORBIDDENT_REQUEST,
        // 文件请求
        FILE_REQUEST,
        // 内部错误
        INTERNAL_ERROR,
        // 连接关闭
        CLOSED_CONNECTION,
    };

    HttpRequest();
    ~HttpRequest() = default;

    // 初始化
    void Init();

    // 解析请求
    bool parse(Buffer &buff);

    // 获取请求的路径
    std::string path() const;
    std::string &path();

    // 获取请求的方法
    std::string method() const;

    // 获取请求的版本
    std::string version() const;

    // 获取POST请求中指定键的值
    std::string GetPost(const std::string &key) const;
    std::string GetPost(const char *key) const;

    // 判断请求是否保持连接
    bool IsKeepAlive() const;

private:
    // 解析请求行
    bool ParseRequestLine_(const std::string &line);

    // 解析请求头部
    void ParseHeader_(const std::string &line);

    // 解析消息体
    void ParseBody_(const std::string &line);

    // 解析请求路径
    void ParsePath_();

    // 解析POST请求
    void ParsePost_();

    // 解析URL编码的数据
    void ParseFromUrlencoded_();

    // 用户验证，isLogin未0注册，为1登陆
    static bool UserVerify(const std::string &name, const std::string &pwd, bool isLogin);

    PARSE_STATE state_;

    // 请求方法，路径，版本，消息体
    std::string method_, path_, version_, body_;

    // 请求头
    std::unordered_map<std::string, std::string> header_;

    // POST请求键值对
    std::unordered_map<std::string, std::string> post_;

    // 默认的HTML页面
    static const std::unordered_set<std::string> DEFAULT_HTML;

    // 默认的HTML标签
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

    // 字符转换：十六进制->十进制
    static int ConverHex(char ch);
};

#endif