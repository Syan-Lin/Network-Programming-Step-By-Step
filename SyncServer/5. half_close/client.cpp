#include <iostream>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../include/error_handler.h"

using namespace std;

class Client {
public:
    Client() {
        memset(&addr, 0, sizeof(addr));
    }

    void init(string ip, int port) {
        // 设置地址
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);

        // 创建套接字
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
        int file_size = 0;
        if(recv(sock, &file_size, sizeof(file_size), 0) == -1) {
            ErrorHandler::handle(ERROR::RECV_ERROR);
            return;
        }

        char* file = new char[file_size];
        recv(sock, file, file_size, 0);
        ::shutdown(sock, SHUT_RD); // 关闭读入流, 不再接收信息

        // 文件相关操作
        ofstream outFile("hello_client", ios::out | ios::binary | ios::trunc);
        outFile.write(file, file_size);
        outFile.close();
        delete[] file;

        string mess = "File download succeeded!";
        cout << mess << "(size: " << file_size << ")" << endl;

        // 发送数据
        if(send(sock, mess.c_str(), mess.size(), 0) == -1) {
            ErrorHandler::handle(ERROR::SEND_ERROR);
        }
    }

private:
    struct sockaddr_in addr;
    int sock;
};

int main() {
    Client client;
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}

// Note: close 套接字意味着套接字会被销毁, 且在后续可以被复用
// 而 shutdown 会对流进行控制, 并不会销毁套接字, 即使是 SHUT_RDWR
// 所以最后仍然需要 close 套接字

// close 的问题:
// 1. close 只是将 socket 描述字的访问计数减 1, 仅当描述字的访问计数为 0 时
// 才真正的关闭 socket, 这在 socket 描述字发生复制时需要注意
// 2. close 终止了数据传输的两个方向: 读与写, 而 TCP 管道是全双工的, 有时候
// 我们仅仅是想通知另一端我们已经完成了数据的发送, 但还想继续接收另一端发送过
// 来的数据, 在这种情况下调用 close 关闭 socket 是不合适的