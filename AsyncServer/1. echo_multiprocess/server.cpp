#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

using namespace std;

#define MAX_BUFF 1024

// 打印信息, 同时解决僵尸进程问题
void child_done(int sig) {
    int status;
    // -1 表示任一子进程 status 储存状态信息
    pid_t pid = waitpid(-1, &status, WNOHANG);  // WNOHANG 非阻塞
    WIFEXITED(status);      // 子进程正常终止返回 true
    WEXITSTATUS(status);    // 子进程的返回值
    cout << "process id: " << pid << " done." << endl;
}

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
        addr_serv.sin_family = AF_INET;					// AF_INET:    IPv4
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY: 监听所有 IP 地址
        addr_serv.sin_port = htons(port);				// htons:      本地序 -> 网络序

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

        // 进程设置
        struct sigaction act;
        act.sa_handler = child_done;
        sigemptyset(&act.sa_mask);  // 初始化为 0
        act.sa_flags = 0;           // 初始化为 0
        if(sigaction(SIGCHLD, &act, 0) < 0) { // 向操作系统注册信号, 成功返回 0
            error_handler(SIGNAL);
        }
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
            if(socket_clnt == -1) { continue; } // 在客户端关闭连接之后, sigaction 会发出中断
                                                // 使 accept 直接返回 -1

            string ipstr(inet_ntoa(addr_clnt.sin_addr));
            cout << "A connection from " << ipstr << endl;

            // 业务
            business();

            // 关闭客户端连接
            close(socket_clnt);
        }
    }

    void shutdown() {
        if(error == SOCKET_ERROR) return;
        close(socket_serv);
    }

private:
    void business() {
        pid_t pid = fork();
        if(pid == 0) { // 子进程, 子进程复制父进程的内存, 会导致有多个文件描述符对
                       // 应同一个套接字, 只有所有文件描述符销毁之后才会销毁套接字
            business_process();
        }
    }

    void business_process() {
        while(true) {
            // 接收数据
            char raw_data[MAX_BUFF];
            memset(raw_data, 0, sizeof(raw_data));
            int rtn_val = recv(socket_clnt, raw_data, sizeof(raw_data), 0);
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
                continue;
            }
            string data(raw_data);
            if(data == "Quit" || rtn_val == 0) { break; } // 断开连接或主动退出
            cout << "pid " << getpid() << " Receive data: " + data << endl;

            // 发送数据
            if(send(socket_clnt, data.c_str(), data.size(), 0) == -1) {
                error_handler(SEND_ERROR);
            }
        }
        // 关闭连接, 子进程会各复制一份 sock_server 和 sock_client, 需要关闭
        close(socket_clnt);
        close(socket_serv);
        cout << "pid " << getpid() << " disconnect" << endl;
        exit(0); // 结束子进程
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;

private:
    enum ERROR { NUll = 0, SOCKET_ERROR, BIND_ERROR, LISTEN_ERROR, SEND_ERROR, RECV_ERROR, SIGNAL };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR: cout << "socket error" << endl; break;
            case BIND_ERROR:   cout << "bind error" << endl;   break;
            case LISTEN_ERROR: cout << "listen error" << endl; break;
            case SEND_ERROR:   cout << "send error" << endl;   break;
            case RECV_ERROR:   cout << "recv error" << endl;   break;
            case SIGNAL:       cout << "signal error" << endl; break;
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

// Note:
// 1. 要注意 fork 会复制父进程的内存空间, 导致有多个文件描述符
// 2. sigaction 是为了让子进程结束时通知父进程, 解决僵尸进程问题
// 3. accept 在客户端关闭连接时也会返回值

// Q: 本地多客户端, 在服务端输出 socket_clnt 一样?
// A: 每个进程分配的 socket 标识符是独立的, 不同进程可以拥有相同的文件描述符
//    每次调用 accpet 都返回相同的值(因为 close 了)
//    但是在操作系统层面实际上是不一样的