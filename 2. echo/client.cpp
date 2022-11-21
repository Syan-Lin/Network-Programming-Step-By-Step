#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ client.cpp -o client

//                                                +------------+
//                                                |            |
//                                                v            |
// 客户端流程 [创建 socket] -> [连接服务器] -> [发送数据] -> [接收数据] -> [关闭连接] -> [退出]
//                                                |                          ^
//                                                |                          |
//                                                +----------if(Q)-----------+

#define MAX_BUFF 1024

class Client {
public:
    Client() {
        memset(&addr, 0, sizeof(addr));
    }

    void init(string ip, int port) {
        // 1. 设置协议、地址、端口
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);

        // 2. 创建套接字，参数(协议族，传输方式，协议)
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock == -1) {
            error_handler(SOCKET_ERROR);
            return;
        }
    }

    void run() {
        if(error > 0) return;
        // 1. 连接服务器
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            error_handler(CONNECT_ERROR);
            return;
        }
        cout << "Connect to server" << endl;

        // 2. 业务
        business();
    }

    void shutdown() { // 关闭连接
        if(error == SOCKET_ERROR) return;
        close(sock);
    }

private:
    struct sockaddr_in addr;
    int sock;

    void business() {
        while(true) {
            string mess;
            cout << "Sent data: ";
            cin >> mess;

            // 1. 发送数据
            if(send(sock, mess.c_str(), mess.size(), 0) == -1) {
                error_handler(SEND_ERROR);
                continue;
            }

            if(mess == "Quit") { break; }

            // 2. 接收数据
            char raw_data[MAX_BUFF];
            memset(raw_data, 0, sizeof(raw_data));
            int rtn_val = recv(sock, raw_data, sizeof(raw_data), 0);
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
            } else if(rtn_val == 0) { // 服务器断开连接
                error_handler(DISCONNECT);
                break;
            }
            string data(raw_data);

            cout << "Receive data: " + data << endl;
        }
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, CONNECT_ERROR, SEND_ERROR, RECV_ERROR, DISCONNECT };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR:  cout << "socket error" << endl;      break;
            case CONNECT_ERROR: cout << "bind error" << endl;        break;
            case SEND_ERROR:    cout << "send error" << endl;        break;
            case RECV_ERROR:    cout << "recv error" << endl;        break;
            case DISCONNECT:    cout << "server disconnect" << endl; break;
        }
    }
};

int main() {
    Client client;
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}

// Note: recv 是阻塞完成的，即使服务端发送一段很长的数据，超过一个报文所
// 能携带的最长数据，recv 也会阻塞直到接收到完整的数据。