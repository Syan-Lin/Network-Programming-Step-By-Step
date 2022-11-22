#include <iostream>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

// hello_server 由 hello.cpp 编译而来的可执行二进制文件, 发送后可执行来校验文件是否正确

#define MAX_BUFF 1024

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
            error_handler(SOCKET_ERROR);
            return;
        }

        // 绑定地址
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            error_handler(BIND_ERROR);
            return;
        }

        socklen_t temp = sizeof(socket_serv);
        setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, nullptr, temp);
    }

    void run() {
        if(error > 0) return;
        // 开始监听
        if(listen(socket_serv, 8) == -1) {
            error_handler(LISTEN_ERROR);
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

            // 关闭客户端连接
            close(socket_clnt);
            cout << "Disconnect " << ipstr << endl;
        }
    }

    void shutdown() {
        if(error == SOCKET_ERROR) return;
        close(socket_serv);
    }

private:
    void business() {
        // 文件相关操作
        ifstream inFile("hello_server",ios::in | ios::binary);
        inFile.seekg(0, std::ios_base::end);
        int file_size = inFile.tellg();
        inFile.seekg(0, std::ios_base::beg);

        char* file = new char[file_size];
        inFile.read(file, file_size);
        inFile.close();

        // 发送数据
        if(send(socket_clnt, &file_size, sizeof(file_size), 0) == -1
            || send(socket_clnt, file, file_size, 0) == -1) {
            error_handler(SEND_ERROR);
            return;
        }
        cout << "Send file..." << "(size: " << file_size << ")" << endl;
        ::shutdown(socket_clnt, SHUT_WR); // 关闭输出流
        delete file;

        // 接收数据
        char raw_data[MAX_BUFF];
        memset(raw_data, 0, sizeof(raw_data));
        if(recv(socket_clnt, raw_data, sizeof(raw_data), 0) == -1) {
            error_handler(RECV_ERROR);
            return;
        }
        string data(raw_data);
        cout << "Receive data: " + data << endl;
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;

private:
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

// Note: close 套接字意味着套接字会被销毁, 且在后续可以被复用
// 而 shutdown 会对流进行控制, 并不会销毁套接字, 即使是 SHUT_RDWR
// 所以最后仍然需要 close 套接字