#include "httprequest.hpp"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "picture",
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

HttpRequest::HttpRequest()
{
    Init();
}

int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

void HttpRequest::Init()
{
    method_ = path_ = version_ = body_ = "";
    // 先解析请求行
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    return false;
}

bool HttpRequest::parse(Buffer &buff)
{
    const char CRLF[] = "\r\n";
    // 缓冲区无可读数据
    if (buff.ReadableBytes() <= 0)
    {
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH)
    {
        // 获取第一个换行符出现的位置
        const char *lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // 拿到一行数据
        std::string line(buff.Peek(), lineEnd);
        switch (state_)
        {
        case REQUEST_LINE:
            if (!ParseRequestLine_(line))
                return false;
            ParsePath_();
            break;
        case HEADERS:
            ParseHeader_(line);
            // 如果没有消息体数据
            if (buff.ReadableBytes() <= 2)
                state_ = FINISH;
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if (lineEnd == buff.BeginWrite())
            break;
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

bool HttpRequest::ParseRequestLine_(const std::string &line)
{
    // 正则表达式匹配
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    // 存放匹配结果
    std::smatch subMatch;
    // 匹配成功
    if (std::regex_match(line, subMatch, patten))
    {
        // subMatch[0]包含整个匹配组
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        // 请求行接着头部信息
        state_ = HEADERS;
        return true;
    }
    // 失败
    LOG_ERROR("RequestLine Error!");
    return false;
}

void HttpRequest::ParsePath_()
{
    if (path_ == "/")
        path_ = "/index.html";
    else
    {
        for (auto &item : DEFAULT_HTML)
        {
            if (item == path_)
            {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParseHeader_(const std::string &line)
{
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    // 匹配头部键值对
    if (std::regex_match(line, subMatch, patten))
        header_[subMatch[1]] = subMatch[2];
    else
        state_ = BODY;
}

void HttpRequest::ParseBody_(const std::string &line)
{
    // 消息体，一般存放POST请求提交的表单数据
    body_ = line;
    // 解析表单数据
    ParsePost_();
    // 只有一行
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

void HttpRequest::ParsePost_()
{
    //"application/x-www-form-urlencoded"：常见表单提交方式
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        ParseFromUrlencoded_();
        // 如果是登陆或者注册页面
        if (DEFAULT_HTML_TAG.count(path_))
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1)
            {
                // 是否是登陆界面
                bool isLogin = (tag == 1);
                // 用户登陆或者注册
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
}

// 解析URL
void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
        return;

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            // 得到键
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            // URL中'+'表示空格
            body_[i] = ' ';
            break;
        case '%':
            // '%'后跟两个十六进制数表示字符
            // 不能解析中文
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            // 得到值
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if (post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin)
{
    if (name == "" || pwd == "")
        return false;

    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;
    // 连接数据库
    // SqlConnRAII(&sql, SqlConnPool::Instance());
    SqlConnRAII sqlraii(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    // unsigned int j = 0;
    char order[256] = {0};
    // MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin)
        flag = true;
    // 查询用户及密码
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // mysql查询，查询成功返回0
    if (mysql_query(sql, order))
    {
        // 释放结果集
        mysql_free_result(res);
        return false;
    }
    // 获取结果集
    res = mysql_store_result(sql);
    // 获取结果集字段数目
    // j = mysql_num_fields(res);
    // 获取结果集中字段信息
    // fields = mysql_fetch_fields(res);
    // 获取一行数据
    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        if (isLogin)
        {
            if (pwd == password)
                // 登陆且密码正确
                flag = true;
            else
            {
                // 登陆且密码错误
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else
        {
            // 注册且用户名已经存在
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    // 释放结果集
    mysql_free_result(res);

    // 注册行为 且 用户名未被使用
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        // 清空sql语句
        bzero(order, 256);
        // 注册用户
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const
{
    return path_;
}

std::string &HttpRequest::path()
{
    return path_;
}
std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

std::string HttpRequest::GetPost(const std::string &key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char *key) const
{
    assert(key != nullptr);
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}