#include "HttpUtil.h"

std::string HttpUtil::getMimeType(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) return "text/plain; charset=utf-8";
    
    std::string ext = filename.substr(dotPos);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    return "text/plain; charset=utf-8";
}

std::string HttpUtil::urlDecode(const std::string& src) {
    std::string dst;
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length()) {
            try {
                int value = std::stoi(src.substr(i + 1, 2), nullptr, 16);
                dst += static_cast<char>(value);
                i += 2;
            } catch (...) {
                dst += src[i]; // 解析失败按原字符处理
            }
        } else {
            dst += src[i];
        }
    }
    return dst;
}