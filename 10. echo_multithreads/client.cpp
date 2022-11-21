#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ client.cpp -o client

// I/O 分离
//                                          [发送数据] ---if(Q)--> [关闭连接]
//                                              ^                      |
//                                              |                      v
// 客户端流程 [创建 socket] -> [连接服务器] -> fork()                 [退出]
//                                              |
//                                              v
//                                          [接收数据]

#define MAX_BUFF 1024

class Client {
public:
    Client(string name) : name(name) {
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
    string name;
    int sock;

    // 多线程相关
    mutex loc;

    void business() {
        thread write_thread(&Client::business_write, this); // 子线程负责发送数据
        business_read(); // 主线程负责接收数据
        write_thread.detach();
    }

    void business_write() {
        while(true) {
            if(error == DISCONNECT) return;
            this_thread::sleep_for(chrono::seconds(1)); // 为了控制台信息不乱
            string mess;
            cout << "sent data: ";
            cin >> mess;
            mess = "[" + name + "]: " + mess;

            if(send(sock, mess.c_str(), mess.size(), 0) == -1) {
                error_handler(SEND_ERROR);
                return;
            }

            if(mess == "Quit") {
                return;
            }
        }
    }

    void business_read() {
        while(true) {
            char raw_data[MAX_BUFF];
            memset(raw_data, 0, sizeof(raw_data));

            int rtn_val = recv(sock, raw_data, sizeof(raw_data), 0);
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
                return;
            } else if(rtn_val == 0) { // 服务器断开连接
                error_handler(DISCONNECT);
                return;
            }

            string data(raw_data);
            cout << data << endl;
        }
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, CONNECT_ERROR, SEND_ERROR, RECV_ERROR, DISCONNECT };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        unique_lock<mutex> ul(loc);
        error = code;
        switch(code) {
            case SOCKET_ERROR:  cout << "socket error" << endl;      break;
            case CONNECT_ERROR: cout << "connect error" << endl;     break;
            case SEND_ERROR:    cout << "send error" << endl;        break;
            case RECV_ERROR:    cout << "recv error" << endl;        break;
            case DISCONNECT:    cout << "server disconnect" << endl; break;
        }
    }
};

int main() {
    cout << "name: ";
    string name;
    cin >> name;
    Client client(name);
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}