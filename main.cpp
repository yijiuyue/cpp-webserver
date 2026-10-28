#include "Server.h"
#include <signal.h>

int main() {
    // 忽略 SIGPIPE，防止发送数据给已断开的客户端导致进程崩溃
    signal(SIGPIPE, SIG_IGN);
    
    Server server(8889); // 端口号设为 8889
    server.start();
    
    return 0;
}