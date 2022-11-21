#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ server.cpp -o server

//                                              +---------------------------------------+
//                                              |                                       |
//                                              v                                       |
// 服务端流程 [创建 socket] -> [绑定地址] -> [监听连接] -> [接收数据] -> [发送数据] -> [关闭连接] -> [退出]
//                                                           |                          ^
//                                                           |                          |
//                                                           +----------if(Q)-----------+

#define MAX_BUFF 1024

class Server {
public:
    Server() {
        memset(&addr_serv, 0, sizeof(addr_serv));
        memset(&addr_clnt, 0, sizeof(addr_clnt));
        FD_ZERO(&reads);
        len = sizeof(addr_clnt);
    }
    Server(const Server&)            = delete;
    Server(Server&&)                 = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&)      = delete;

    void init(int port) {
        // 1. 设置协议、监听端口和地址
        addr_serv.sin_family = AF_INET;					// AF_INET:    IPv4
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY: 监听所有 IP 地址
        addr_serv.sin_port = htons(port);				// htons:      本地序 -> 网络序

        // 2. 创建套接字，参数(协议族，传输方式，协议)
        socket_serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(socket_serv == -1) {
            error_handler(SOCKET_ERROR);
            return;
        }

        // 3. 将地址绑定至套接字
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            error_handler(BIND_ERROR);
            return;
        }

        // 跳过 Time_wait 这一阶段, 使服务器可以立即重启
        socklen_t temp = sizeof(socket_serv);
        setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, nullptr, temp);

        // select 相关参数
        FD_SET(socket_serv, &reads);
        fd_max = socket_serv;
    }

    void run() {
        if(error > 0) return;
        // 开始监听，第二个参数代表半连接队列的最长长度
        if(listen(socket_serv, 8) == -1) {
            error_handler(LISTEN_ERROR);
            return;
        }
        cout << "Server is running!" << endl;
        while(true) {
            fd_set cpy_reads = reads;
            timeout.tv_sec = 1;     // 每次都要设置阻塞时间
            timeout.tv_usec = 0;    // 毫秒

            // fd_max 表示的是最大的下标, 而 select 第一个参数表示数量, 所以需要 + 1
            int fd_num = select(fd_max + 1, &cpy_reads, nullptr, nullptr, &timeout);
            if(fd_num == -1) { // select 出错
                error_handler(SELECT_ERROR);
                break;
            } else if(fd_num == 0) { // 当前没有套接字产生事件
                continue;
            }

            for(int sock = 0; sock < fd_max + 1; sock++) {
                if(FD_ISSET(sock, &cpy_reads)) {
                    if(sock == socket_serv) {
                        add_socket();
                    } else {
                        business(sock);
                    }
                }
            }
        }
    }

    void shutdown() { // 关闭套接字
        if(error == SOCKET_ERROR) return;
        close(socket_serv);
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;

    // select 相关参数
    struct timeval timeout; // select 阻塞等待时间
    fd_set reads;           // 监听接收数据事件, 表示有数据需要读
    int fd_max;

    void business(int sock) {
        // 接收数据
        char raw_data[MAX_BUFF];
        memset(raw_data, 0, sizeof(raw_data));
        int rtn_val = recv(socket_clnt, raw_data, sizeof(raw_data), 0);
        if(rtn_val == -1) {
            error_handler(RECV_ERROR);
            return;
        }

        string data(raw_data);
        cout << "[socket " << sock << "] receive data: " + data << endl;
        if(data == "Quit" || rtn_val == 0) { // 断开连接或主动退出
            // 注销套接字
            FD_CLR(sock, &reads);
            close(sock);
            cout << "[socket " << sock << "] disconnect" << endl;
        } else {
            // 发送数据
            if(send(sock, data.c_str(), data.size(), 0) == -1) {
                error_handler(SEND_ERROR);
            }
        }
    }

    void add_socket() {
        // 连接请求通过数据传输完成, 服务端套接字有数据需要读, 代表连接请求
        // 获取来自客户端的连接，如果没有则会阻塞
        socket_clnt = accept(socket_serv, nullptr, nullptr);

        // 加入套接字集合
        if(socket_clnt != -1) {
            FD_SET(socket_clnt, &reads);
            if(socket_clnt > fd_max) {
                fd_max = socket_clnt;
            }

            cout << "[socket " << socket_clnt << "] connect" << endl;
        }
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, BIND_ERROR, LISTEN_ERROR, SEND_ERROR, RECV_ERROR, SELECT_ERROR };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR: cout << "socket error" << endl; break;
            case BIND_ERROR:   cout << "bind error" << endl;   break;
            case LISTEN_ERROR: cout << "listen error" << endl; break;
            case SEND_ERROR:   cout << "send error" << endl;   break;
            case RECV_ERROR:   cout << "recv error" << endl;   break;
            case SELECT_ERROR: cout << "select error" << endl; break;
        }
    }
};

int main() {
    Server server;
    server.init(6666);
    server.run();
    server.shutdown();

    return 0;
}

// Note: select 阻塞等待时间指在没有任何事件时, select 阻塞多久才返回值
// 当有事件时就不会阻塞