#include "Server.h"
#include "HttpUtil.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <filesystem> 
#include <cstring>

namespace fs = std::filesystem;

Server::Server(int port) : port_(port), lfd_(-1), epfd_(-1), pool_(4) {}

Server::~Server() {
    if (lfd_ != -1) close(lfd_);
    if (epfd_ != -1) close(epfd_);
}

void Server::initSocket() {
    // 1. 创建 Socket
    lfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd_ < 0) {
        perror("socket");
        exit(1);
    }

    // 端口复用
    int opt = 1;
    setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(lfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // 3. Listen
    if (listen(lfd_, 128) < 0) {
        perror("listen");
        exit(1);
    }

    // 4. 创建 Epoll 树
    epfd_ = epoll_create1(0);
    if (epfd_ < 0) {
        perror("epoll_create1");
        exit(1);
    }

    // 5. 将监听 fd 挂上树
    struct epoll_event ev;
    ev.data.fd = lfd_;
    ev.events = EPOLLIN;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, lfd_, &ev);
}

void Server::start() {
    initSocket();
    std::cout << "Server listening on port " << port_ << std::endl;

    struct epoll_event evs[1024];
    while (true) {
        int nready = epoll_wait(epfd_, evs, 1024, -1);
        if (nready < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; ++i) {
            if (evs[i].data.fd == lfd_) {
                handleAccept();
            } else if (evs[i].events & EPOLLIN) {
                 int cfd = evs[i].data.fd;
                 pool_.enqueue([this, cfd]() {
                    this->handleRead(cfd);
                 });
            }
        }
    }
}

void Server::handleAccept() {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    int cfd = accept(lfd_, (struct sockaddr*)&cliaddr, &len);
    if (cfd < 0) return;

    // 设置非阻塞
    int flag = fcntl(cfd, F_GETFL);
    fcntl(cfd, F_SETFL, flag | O_NONBLOCK);

    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, cfd, &ev);
    
    std::cout << "New client connected: " << cfd << std::endl;
     // 3. 初始化这个连接的 Buffer
    buffers_[cfd] = Buffer(); 
}

void Server::handleRead(int cfd) {
     bool needClose = false; 
    while (true) {
        // 使用 Buffer 去读
        ssize_t n = buffers_[cfd].readFd(cfd);
        
        if (n > 0) {
            // 检查是否收到了完整的 HTTP 请求头（\r\n\r\n）
            const char* crlf = buffers_[cfd].findCRLFCRLF();
            if (crlf) {
                // 收全了，把数据取出来，去处理业务
                std::string reqStr = buffers_[cfd].retrieveAllAsString();
                processRequest(cfd, reqStr); 
                 needClose = true; // 业务处理完了，短连接需要关闭
                break; // 短连接，处理完退出循环
            }
            // 如果没找到 \r\n\r\n，说明半包了，继续 loop 去 recv
        } 
        else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据彻底读空了，退出循环，安心等下一次 EPOLLIN 事件
                break; 
            } else {
                // 真正的错误或客户端断开
                 needClose = true; // 读出错，需要关闭
                break;
            }
        }
        else if (n == 0) {
            // 客户端断开
           needClose = true; // 客户端断开，需要关闭
            break;
        }
        if (needClose) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, cfd, nullptr);
        close(cfd);
        buffers_.erase(cfd);
    }
    }
   
   
}
void Server::processRequest(int cfd, const std::string& reqStr) {
   std::istringstream iss(reqStr);
    std::string method, path, version;
    iss >> method >> path >> version;

    // 2. 只处理 GET 请求
    if (method != "GET") {
        std::string err = "405 Method Not Allowed";
        sendHeader(cfd, 405, "Method Not Allowed", "text/html", err.length());
        send(cfd, err.c_str(), err.length(), 0);
        close(cfd);
        epoll_ctl(epfd_, EPOLL_CTL_DEL, cfd, nullptr);
        buffers_.erase(cfd);
        return;
    }

    // 3. 处理路径
    path = HttpUtil::urlDecode(path);
    if (path == "/") path = "/index.html";
    std::string filepath = "./web_root" + path;

    // 4. 文件状态判断
    struct stat s;
    if (stat(filepath.c_str(), &s) < 0) {
        std::string err = "<h1>404 Not Found</h1>";
        sendHeader(cfd, 404, "Not Found", "text/html", err.length());
        send(cfd, err.c_str(), err.length(), 0);
    } 
    else if (S_ISREG(s.st_mode)) {
        sendHeader(cfd, 200, "OK", HttpUtil::getMimeType(filepath), s.st_size);
        sendFile(cfd, filepath);
    } 
    else if (S_ISDIR(s.st_mode)) {
        std::string html = "<html><body><h1>Directory Listing</h1><ul>";
        for (const auto& entry : fs::directory_iterator(filepath)) {
            std::string name = entry.path().filename().string();
            html += "<li><a href='" + name + "'>" + name + "</a></li>";
        }
        html += "</ul></body></html>";
        sendHeader(cfd, 200, "OK", "text/html", html.length());
        send(cfd, html.c_str(), html.length(), 0);
    }

    // 5. 处理完毕，关闭连接（短连接模式）
    close(cfd);
    epoll_ctl(epfd_, EPOLL_CTL_DEL, cfd, nullptr);
    buffers_.erase(cfd); // 记得把 Buffer 也清理掉，防止内存泄漏
}

void Server::sendHeader(int cfd, int code, const std::string& info, const std::string& mimeType, int length) {
    std::string header = "HTTP/1.1 " + std::to_string(code) + " " + info + "\r\n";
    header += "Content-Type: " + mimeType + "\r\n";
    if (length > 0) header += "Content-Length: " + std::to_string(length) + "\r\n";
    header += "Connection: close\r\n\r\n";
    send(cfd, header.c_str(), header.length(), 0);
}

void Server::sendFile(int cfd, const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    
    // 将整个文件读入 string
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    send(cfd, content.c_str(), content.length(), 0);
}

//浏览器发起请求，数据到达网卡，主线程卡在epoll_wait，
//如果是lfd就绪（响应），代表有新的要加入，调用accept，将新的cfd设置为非阻塞，上树；
//若是cfd就绪（响应），代表cfd想要读或者写（调用recv，读取数据，暂时没有写的过程），
//把字符串分割到不同string，提取出path，（记得加/）然后判断文件类型(stat)，
//若是普通文件，直接读取内容，组包http回应；若是目录，则需要读取目录下的内容，返回列表；完成后下树；