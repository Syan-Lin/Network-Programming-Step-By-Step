#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ server.cpp -o server

//                                              +---------------------------------------+
//                                              |                                       |
//                                              v                                       |
// 服务端流程 [创建 socket] -> [绑定地址] -> [监听连接] -> [接收数据] -> [发送数据] -> [关闭连接] -> [退出]
//                                                           |                                      ^
//                                                           |                                      |
//                                                           +-----------if(count <= 0)-------------+

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
            // 1. 获取来自客户端的连接，如果没有则会阻塞，后两个参数用来接受客户端的 IP 信息
            socket_clnt = accept(socket_serv, (struct sockaddr*)&addr_clnt, &len);
            if(socket_clnt == -1) { continue; }

            string ipstr(inet_ntoa(addr_clnt.sin_addr));
            cout << "A connection from " << ipstr << endl;

            // 2. 业务
            business();

            // 3. 关闭连接
            close(socket_clnt);
            cout << "Disconnect " << ipstr << endl;
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

    void business() {
        while(true) {
            // 1. 接收数据
            int count;
            int rtn_val = recv(socket_clnt, &count, sizeof(count), 0);
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
                continue;
            } else if(rtn_val == 0) { // 断开连接
                break;
            }

            // 姑且认为第一次调用 recv 正常后续就正常
            int* data = new int[count];
            recv(socket_clnt, data, sizeof(int) * count, 0);
            for(int i = 0; i < count; i++) {
                cout << "data " << i + 1 << ": " << data[i] << endl;
            }
            char opt;
            recv(socket_clnt, &opt, sizeof(char), 0);
            cout << "operator: " << opt << endl;

            int result = data[0];
            for(int i = 1; i < count; i++){
                switch(opt) {
                    case '+':
                        result += data[i];
                        break;
                    case '-':
                        result -= data[i];
                        break;
                    case '*':
                        result *= data[i];
                        break;
                    case '/':
                        if(data[i] != 0)
                            result /= data[i];
                        break;
                }
            }
            cout << "result: " << result << endl;

            // 2. 发送数据
            if(send(socket_clnt, &result, sizeof(result), 0) == -1) {
                error_handler(SEND_ERROR);
            }
        }
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, BIND_ERROR, LISTEN_ERROR, SEND_ERROR, RECV_ERROR };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR: cout << "socket error" << endl; break;
            case BIND_ERROR:   cout << "bind error" << endl;   break;
            case LISTEN_ERROR: cout << "listen error" << endl; break;
            case SEND_ERROR:   cout << "send error" << endl;   break;
            case RECV_ERROR:   cout << "recv error" << endl;   break;
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

// Note: 虽然客户端只调用了一次 send, 但是服务端仍然可以调用多次 recv
// 每次只获得指定字节数的数据