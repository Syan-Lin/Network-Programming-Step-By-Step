#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include "../../include/error_handler.h"

using namespace std;

#define MAX_BUFF 1024

class Client {
public:
    Client(string name) : name(name) {
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
        thread write_thread(&Client::business_write, this); // 子线程负责发送数据
        write_thread.detach();
        business_read(); // 主线程负责接收数据
    }

    void business_write() {
        while(true) {
            {
                unique_lock<mutex> ul(loc);
                if(ErrorHandler::error == ERROR::DISCONNECT) return;
            }

            string input;
            cin >> input;
            string mess = "[" + name + "]: " + input;

            if(send(sock, mess.c_str(), mess.size(), 0) == -1) {
                ErrorHandler::handle(ERROR::SEND_ERROR);
                return;
            }

            if(input == "Quit") {
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
                ErrorHandler::handle(ERROR::RECV_ERROR);
                return;
            } else if(rtn_val == 0) { // 服务器断开连接
                ErrorHandler::handle(ERROR::DISCONNECT);
                return;
            }

            string data(raw_data);
            cout << data << endl;
        }
    }

private:
    struct sockaddr_in addr;
    const string name;
    int sock;

    // 多线程相关
    mutex loc;
};

int main() {
    cout << "Your name: ";
    string name;
    cin >> name;

    Client client(name);
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}