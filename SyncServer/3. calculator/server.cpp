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
        // 设置地址
        addr_serv.sin_family = AF_INET;
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_serv.sin_port = htons(port);

        // 创建套接字
        socket_serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(socket_serv == -1) {
            ErrorHandler::handle(ERROR::SOCKET_ERROR);
            return;
        }

        // 绑定地址
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            ErrorHandler::handle(ERROR::BIND_ERROR);
            return;
        }

        socklen_t temp = sizeof(socket_serv);
        setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, nullptr, temp);
    }

    void run() {
        if(ErrorHandler::error != ERROR::NO_ERROR) return;
        // 开始监听
        if(listen(socket_serv, 8) == -1) {
            ErrorHandler::handle(ERROR::LISTEN_ERROR);
            return;
        }

        cout << "Server is running!" << endl;
        while(true) {
            // 获取来自客户端的连接
            socket_clnt = accept(socket_serv, (struct sockaddr*)&addr_clnt, &len);
            if(socket_clnt == -1) { continue; }

            string ipstr(inet_ntoa(addr_clnt.sin_addr));
            cout << "A connection from " << ipstr << endl;

            // 业务
            business();

            // 关闭连接
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
        while(true) {
            // 接收数据
            int count;
            int rtn_val = recv(socket_clnt, &count, sizeof(count), 0);
            if(rtn_val == -1) {
                ErrorHandler::handle(ERROR::RECV_ERROR);
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

            // 发送结果
            if(send(socket_clnt, &result, sizeof(result), 0) == -1) {
                ErrorHandler::handle(ERROR::SEND_ERROR);
            }
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

// Note: 虽然客户端只调用了一次 send, 但是服务端仍然可以调用多次 recv
// 每次只获得指定字节数的数据