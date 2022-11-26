#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../include/error_handler.h"

using namespace std;

#define MAX_BUFF 1024

class Client {
public:
    Client() {
        memset(&addr, 0, sizeof(addr));
    }

    void init(string ip, int port) {
        // 设置协议、地址、端口
        addr.sin_family = AF_INET;					    // AF_INET:  IPv4
        addr.sin_addr.s_addr = inet_addr(ip.c_str());	// ip:       连接的 IP 地址
        addr.sin_port = htons(port);				    // htons:    本地序 -> 网络序

        // 创建套接字，参数(协议族，传输方式，协议)
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock == -1) {
            ErrorHandler::handle(ERROR::SOCKET_ERROR);
            return;
        }
    }

    void run() {
        if(ErrorHandler::error != ERROR::NO_ERROR) return;
        // 连接服务器
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ErrorHandler::handle(ERROR::CONNECT_ERROR);
            return;
        }
        cout << "Connect to server" << endl;

        // 业务
        business();
    }

    void shutdown() {
        if(sock == -1) return;
        close(sock);
    }

private:
    void business() {
        // 接收数据
        char raw_data[MAX_BUFF];
        memset(raw_data, 0, sizeof(raw_data));
        if(recv(sock, raw_data, sizeof(raw_data), 0) == -1) {
            ErrorHandler::handle(ERROR::RECV_ERROR);
        }
        string data(raw_data);

        cout << "Receive data: " + data << endl;
    }

private:
    struct sockaddr_in addr;
    int sock;
    ErrorHandler eh;
};

int main() {
    Client client;
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}

// Note: recv 是阻塞完成的, 即使服务端发送一段很长的数据, 超过一个报文所
// 能携带的最长数据, recv 也会阻塞直到接收到完整的数据。
// 但如果 recv 的缓存太小, 只调用一次会丢失数据。