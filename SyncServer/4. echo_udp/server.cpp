#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../include/error_handler.h"

using namespace std;

#define MAX_BUFF 1024

class Server_UDP {
public:
    Server_UDP() {
        memset(&addr_serv, 0, sizeof(addr_serv));
        memset(&addr_clnt, 0, sizeof(addr_clnt));
        len = sizeof(addr_clnt);
    }
    Server_UDP(const Server_UDP&)            = delete;
    Server_UDP(Server_UDP&&)                 = delete;
    Server_UDP& operator=(const Server_UDP&) = delete;
    Server_UDP& operator=(Server_UDP&&)      = delete;

    void init(int port) {
        // 设置地址
        addr_serv.sin_family = AF_INET;
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_serv.sin_port = htons(port);

        // 创建 UDP 套接字
        socket_serv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(socket_serv == -1) {
            ErrorHandler::handle(ERROR::SOCKET_ERROR);
            return;
        }

        // 绑定地址
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            ErrorHandler::handle(ERROR::BIND_ERROR);
            return;
        }
    }

    void run() {
        if(ErrorHandler::error != ERROR::NO_ERROR) return;
        cout << "Server is running!" << endl;

        while(true) { // UDP 不需要连接和断开
            // 业务
            business();
        }
    }

    void shutdown() {
        if(socket_serv == -1) return;
        close(socket_serv);
    }

private:
    void business() {
        // 接收数据
        char raw_data[MAX_BUFF];
        memset(raw_data, 0, sizeof(raw_data));
        if(recvfrom(socket_serv, raw_data, sizeof(raw_data), 0, (struct sockaddr*)&addr_clnt, &len) == -1) {
            ErrorHandler::handle(ERROR::RECV_ERROR);
            return;
        }

        string data(raw_data);
        if(data == "Quit") { return; }

        string ipstr(inet_ntoa(addr_clnt.sin_addr));
        cout << "Receive data: " + data + " from " + ipstr << endl;

        // 发送数据
        if(sendto(socket_serv, data.c_str(), data.size(), 0, (struct sockaddr*)&addr_clnt, len) == -1) {
            ErrorHandler::handle(ERROR::SEND_ERROR);
        }
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    socklen_t len;
};

int main() {
    Server_UDP server;
    server.init(6666);
    server.run();
    server.shutdown();

    return 0;
}