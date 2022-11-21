#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ Client.cpp -o Client

//                                +------------+
//                                |            |
//                                v            |
// 客户端流程 [创建 socket] -> [发送数据] -> [接收数据] -> [退出]
//                                |                        ^
//                                |                        |
//                                +----------if(Q)---------+

#define MAX_BUFF 1024

class Client_UDP {
public:
    Client_UDP() {
        memset(&addr, 0, sizeof(addr));
        len = sizeof(addr);
    }

    void init(string ip, int port) {
        // 1. 设置协议、地址、端口
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);

        // 2. 创建套接字，参数(协议族，传输方式，协议)
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(sock == -1) {
            error_handler(SOCKET_ERROR);
            return;
        }
    }

    void run() {
        if(error > 0) return;
        // 1. 连接服务器, 这里和 TCP 的连接服务器实际含义不同, 详见 Note
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            error_handler(CONNECT_ERROR);
            return;
        }

        // 2. 业务
        business();
    }

    void shutdown() { // 关闭连接
        if(error == SOCKET_ERROR) return;
        close(sock);
    }

private:
    struct sockaddr_in addr;
    socklen_t len;
    int sock;

    void business() {
        while(true) {
            // 1. 输入数据
            string mess;
            cout << "Sent data: ";
            cin >> mess;

            // 2. 发送数据
            if(sendto(sock, mess.c_str(), mess.size(), 0, (struct sockaddr*)&addr, len) == -1) {
                error_handler(SEND_ERROR);
                continue;
            }

            if(mess == "Quit") { break; }

            // 3. 接收数据
            char raw_data[MAX_BUFF];
            memset(raw_data, 0, sizeof(raw_data));
            if(recvfrom(sock, raw_data, sizeof(raw_data), 0, (struct sockaddr*)&addr, &len) == -1) {
                error_handler(RECV_ERROR);
                continue;
            }
            string data(raw_data);
            cout << "Receive data: " + data << endl;
        }
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, CONNECT_ERROR, SEND_ERROR, RECV_ERROR };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR:  cout << "socket error" << endl; break;
            case CONNECT_ERROR: cout << "bind error" << endl;   break;
            case SEND_ERROR:    cout << "send error" << endl;   break;
            case RECV_ERROR:    cout << "recv error" << endl;   break;
        }
    }
};

int main() {
    Client_UDP client;
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}

// Note: UDP 的 connect 仅仅是对套接字进行标记, 在发送数据过程中, sendto 函数
// 会自动检查 UDP 套接字是否已经绑定好 IP 和 端口号, 如果没有则会自动进行 IP 和
// 端口号的绑定, 并在发送完数据之后注销, 使 UDP 套接字能够复用
//
// 但如果能够确认该套接字的地址、端口信息不变, 则可以省去不断注册和注销的操作
// 此时的 connect 的具体含义就是绑定信息, 且明确 sendto 不进行注册、注销操作