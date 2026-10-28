#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/uio.h>
#include <algorithm>

class Buffer {
public:
    Buffer() { buffer_.reserve(1024); }

    // 从 socket 读取数据并追加到缓冲区，返回读到的字节数
    ssize_t readFd(int fd) {
        char extrabuf[65536];
        struct iovec vec[2];
        size_t writable = writableBytes();

        vec[0].iov_base = &*buffer_.begin() + readIndex_;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);

        // 使用 readv 读取，避免多次系统调用
        ssize_t n = readv(fd, vec, 2);
        if (n < 0) return -1;
        else if (n <= writable) {
            // 数据全在读缓冲区里，直接调整写指针
        } else {
            // 数据太大了，把 extrabuf 里的数据追加到 buffer_ 尾部
            buffer_.resize(writeIndex_ + n); 
        }
        writeIndex_ += n;
        return n;
    }

    // 查找 HTTP 报头结束标志 \r\n\r\n
    const char* findCRLFCRLF() const {
        const char* start = &*buffer_.begin() + readIndex_;
        const char* end = &*buffer_.begin() + writeIndex_;
        const char* crlf = std::search(start, end, "\r\n\r\n", "\r\n\r\n" + 4);
        return crlf == end ? nullptr : crlf;
    }

    // 取出全部可读数据
    std::string retrieveAllAsString() {
        std::string result(&*buffer_.begin() + readIndex_, readableBytes());
        readIndex_ = writeIndex_;
        return result;
    }

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }

private:
    std::vector<char> buffer_ = std::vector<char>(1024);
    size_t readIndex_ = 0;
    size_t writeIndex_ = 0;
};