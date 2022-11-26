#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>
#include "../../include/error_handler.h"

using namespace std;

#define MAX_BUFF 1024

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
        pid_t pid = fork(); // 分离 IO 流
        if(pid == 0) { // 父进程退出时结束子进程
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        }
        while(true) {
            if(ErrorHandler::error == ERROR::DISCONNECT) return;
            if(pid == 0) { // 子进程负责发送数据
                business_write();
            } else { // 父进程负责接收数据
                business_read();
            }
        }
    }

    void business_write() {
        sleep(1); // 为了控制台信息不乱
        string mess;
        cout << "pid " << getpid() << " sent data: ";
        cin >> mess;

        if(send(sock, mess.c_str(), mess.size(), 0) == -1) {
            ErrorHandler::handle(ERROR::SEND_ERROR);
            return;
        }

        if(mess == "Quit") {
            close(sock);
            exit(0);
        }
    }

    void business_read() {
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
        cout << "pid " << getpid() << " receive data: " + data << endl;
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