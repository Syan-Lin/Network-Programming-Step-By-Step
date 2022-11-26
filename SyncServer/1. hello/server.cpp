#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../include/error_handler.h"

using namespace std;

class Server {
public:
    Server() {
        memset(&addr_serv, 0, sizeof(addr_serv));
        memset(&addr_clnt, 0, sizeof(addr_clnt));
        len = sizeof(addr_clnt);
    }
    Server(const Server&)            = delete;
    Server(Server&&)                 = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&)      = delete;

    void init(int port) {
        // 设置协议、监听端口和地址
        addr_serv.sin_family = AF_INET;					// AF_INET:    IPv4
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY: 监听所有 IP 地址
        addr_serv.sin_port = htons(port);				// htons:      本地序 -> 网络序

        // 创建套接字，参数(协议族，传输方式，协议)
        socket_serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(socket_serv == -1) {
            ErrorHandler::handle(ERROR::SOCKET_ERROR);
            return;
        }

        // 将地址绑定至套接字
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            ErrorHandler::handle(ERROR::BIND_ERROR);
            return;
        }

        // 跳过 Time_wait 这一阶段, 使服务器可以立即重启
        socklen_t temp = sizeof(socket_serv);
        setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, nullptr, temp);
    }

    void run() {
        if(ErrorHandler::error != ERROR::NO_ERROR) return;
        // 开始监听，第二个参数代表半连接队列的最长长度
        if(listen(socket_serv, 8) == -1) {
            ErrorHandler::handle(ERROR::LISTEN_ERROR);
            return;
        }

        cout << "Server is running!" << endl;
        while(true) {
            // 获取来自客户端的连接，如果没有则会阻塞，后两个参数用来接受客户端的 IP 信息
            socket_clnt = accept(socket_serv, (struct sockaddr*)&addr_clnt, &len);
            if(socket_clnt == -1) { continue; }

            string ipstr(inet_ntoa(addr_clnt.sin_addr));
            cout << "A connection from " << ipstr << endl;

            // 业务
            business();

            // 关闭客户端连接
            close(socket_clnt);
            cout << "Disconnect " << ipstr << endl;
        }
    }

    void shutdown() {
        if(socket_serv == -1) return;
        close(socket_serv);
    }

private:
    void business() {
        // 发送数据
        string mess = "hello, welcome to my server!";
        if(send(socket_clnt, mess.c_str(), mess.size(), 0) == -1) {
            ErrorHandler::handle(ERROR::SEND_ERROR);
        }
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;
};

int main() {
    Server server;
    server.init(6666);
    server.run();
    server.shutdown();

    return 0;
}