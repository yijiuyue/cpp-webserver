#pragma once
#include <string>
#include <unordered_map> // 必须引入
#include "Buffer.h"      // 引入你新建的 Buffer.h
#include "ThreadPool.h"  // 引入你新建的 ThreadPool.h
class Server {
public:
    Server(int port);
    ~Server();
    void start();

private:
    void initSocket();
    void handleAccept();
    void handleRead(int cfd);
     void processRequest(int cfd, const std::string& reqStr); // 新增：专门处理业务
    
    void sendHeader(int cfd, int code, const std::string& info, const std::string& mimeType, int length);
    void sendFile(int cfd, const std::string& path);

    int port_;
    int lfd_;
    int epfd_;
    ThreadPool pool_;                               // 线程池对象
    std::unordered_map<int, Buffer> buffers_;       // 为每个 cfd 维护一个独立的读缓冲区
};