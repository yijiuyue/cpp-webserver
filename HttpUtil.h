#pragma once
#include <string>

class HttpUtil {
public:
    // 根据文件后缀获取 MIME 类型
    static std::string getMimeType(const std::string& filename);
    
    // URL 解码 (将 %20 转换为空格等)
    static std::string urlDecode(const std::string& src);
};