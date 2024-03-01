#include "httpresponse.hpp"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse()
{
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse()
{
    UnmapFile();
}

void HttpResponse::UnmapFile()
{
    if (mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

void HttpResponse::Init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code)
{
    assert(srcDir != "");
    if (mmFile_)
    {
        UnmapFile();
    }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

void HttpResponse::MakeResponse(Buffer &buff)
{
    // stat函数获取文件信息,失败返回-1
    // 获取文件信息失败或者该路径是一个文件夹
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
        code_ = 404;
    // 该文件其他用户没有可读权限
    else if (!(mmFileStat_.st_mode & S_IROTH))
        code_ = 403;
    else if (code_ == -1)
        code_ = 200;
    // 生成错误页面
    ErrorHtml_();
    // 添加响应状态
    AddStateLine_(buff);
    // 添加响应头部
    AddHeader_(buff);
    // 添加响应内容
    AddContent_(buff);
}

void HttpResponse::ErrorHtml_()
{
    // 如果状态码不为200，即OK
    if (CODE_PATH.count(code_) == 1)
    {
        // 获得对应错误页面路径
        path_ = CODE_PATH.find(code_)->second;
        // 获取文件状态
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer &buff)
{
    std::string status;
    if (CODE_STATUS.count(code_) == 1)
        status = CODE_STATUS.find(code_)->second;
    else
    {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buff)
{
    buff.Append("Connection: ");
    if (isKeepAlive_)
    {
        // 保持连接
        buff.Append("keep-alive\r\n");
        // 最多六次请求，超时事件为120s
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer &buff)
{
    // 以只读方式打开响应文件
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    // 打开失败
    if (srcFd < 0)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    /*
     *将文件映射到内存提高文件的访问速度 MAP_PRIVATE 建立一个写入时拷贝的私有映射
     *PORT_READ: 可读
     *MAP_PRIVATE: 私有映射，对该内存映射区的写入操作不会影响原文件
     */
    int *mmRet = (int *)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char *)mmRet;
    close(srcFd);
    // 这些实际写的还是头部信息，具体的内容在httpconn中写入
    buff.Append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::ErrorContent(Buffer &buff, std::string message)
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1)
        status = CODE_STATUS.find(code_)->second;
    else
        status = "Bad Request";
    body += std::to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

int HttpResponse::Code() const { return code_; }

char *HttpResponse::File() { return mmFile_; }

size_t HttpResponse::FileLen() const { return mmFileStat_.st_size; }

std::string HttpResponse::GetFileType_()
{
    // 获取文件后缀开始位置
    std::string::size_type idx = path_.find_last_of('.');
    // 获取失败
    if (idx == std::string::npos)
        return "text/plain";
    // 获取文件后缀
    std::string suffix = path_.substr(idx);
    // 获取MIME类型
    if (SUFFIX_TYPE.count(suffix) == 1)
        return SUFFIX_TYPE.find(suffix)->second;
    return "text/plain";
}