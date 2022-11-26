#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <error.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../include/error_handler.h"

using namespace std;

#define MAX_BUFF 1024
#define EPOLL_SIZE 64

class Server {
public:
    Server() {
        memset(&addr_serv, 0, sizeof(addr_serv));
        memset(&addr_clnt, 0, sizeof(addr_clnt));
        len = sizeof(addr_clnt);
    }
    ~Server() {
        delete[] ep_events;
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

        // epoll 相关
        epfd = epoll_create(EPOLL_SIZE); // Linux 内核版本大于 2.6.8 该参数没有意义
        ep_events = new struct epoll_event[EPOLL_SIZE];

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = socket_serv;
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_serv, &event); // 监听服务端套接字
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
            int event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
            if(event_cnt == -1) {
                ErrorHandler::handle(ERROR::EPOLL_ERROR);
                return;
            } else if(event_cnt == 0) {
                continue;
            }

            for(int i = 0; i < event_cnt; i++) {
                if(ep_events[i].data.fd == socket_serv) {
                    add_socket();
                } else {
                    business(ep_events[i].data.fd);
                }
            }
        }
    }

    void shutdown() {
        if(socket_serv != -1)
            close(socket_serv);
        if(epfd != -1)
            close(epfd);
    }

    // 设置边沿触发
    void set_epoll_et(bool val) {
        edge_trigger = val;
    }

private:
    void business(int sock) {
        // 接收数据
        char raw_data[MAX_BUFF];
        char temp[4]; // 模拟缓冲区很小, 一次装不下数据
        memset(raw_data, 0, sizeof(raw_data));
        int rtn_val = 1, begin = 0, times = 0;

        if(edge_trigger) { // 边沿触发 + 非阻塞 IO, 需要手动循环
            while(rtn_val > 0) {
                rtn_val = recv(sock, temp, sizeof(temp), 0);
                times++;
                if(rtn_val == -1) {
                    if(errno == EAGAIN) { // 读完数据, 正常退出
                        cout << "[socket " << sock << "] recv " << times << " times" << endl;
                        break;
                    } else {
                        ErrorHandler::handle(ERROR::RECV_ERROR);
                        return;
                    }
                }
                memcpy((char*)raw_data + begin, temp, rtn_val);
                begin += rtn_val;
            }
        } else { // 水平触发 + 阻塞 IO, 不用循环
            rtn_val = recv(sock, raw_data, sizeof(raw_data), 0);
            if(rtn_val == -1) {
                ErrorHandler::handle(ERROR::RECV_ERROR);
                return;
            }
        }

        string data(raw_data);
        cout << "[socket " << sock << "] receive data: " + data << endl;
        if(data == "Quit" || rtn_val == 0) { // 断开连接或主动退出
            // 注销套接字
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            cout << "[socket " << sock << "] disconnect" << endl;
        } else {
            // 发送数据
            if(send(sock, data.c_str(), data.size(), 0) == -1) {
                ErrorHandler::handle(ERROR::SEND_ERROR);
            }
        }
    }

    void add_socket() {
        // 连接请求通过数据传输完成, 服务端套接字有数据需要读, 代表连接请求
        // 获取来自客户端的连接，如果没有则会阻塞
        socket_clnt = accept(socket_serv, nullptr, nullptr);

        struct epoll_event event;
        if(edge_trigger) { // 边沿触发
            set_no_block(socket_clnt);
            event.events = EPOLLIN | EPOLLET;
        } else {
            event.events = EPOLLIN;
        }

        event.data.fd = socket_clnt;
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_clnt, &event); // 添加客户端套接字监听

        cout << "[socket " << socket_clnt << "] connect" << endl;
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;

    // epoll 相关参数
    struct epoll_event* ep_events; // 储存所有产生的事件
    bool edge_trigger = false;     // epoll 默认为水平触发
    int epfd;

    void set_no_block(int fd) {
        int flag = fcntl(fd, F_GETFL, 0);      // 得到套接字原来属性
        fcntl(fd, F_SETFL, flag | O_NONBLOCK); // 在原有属性基础上设置添加非阻塞模式
    }
};

int main() {
    Server server;
    server.set_epoll_et(true); // 边沿触发
    server.init(6666);
    server.run();
    server.shutdown();

    return 0;
}

// Note:
// 水平触发: 只要输入缓冲有数据就会一直通知该事件
// 边沿触发: 输入缓冲收到数据时仅注册 1 次该事件, 即使输入缓冲中还留有数据
//           也不会再进行注册, 除非状态改变或有新事件
//
// 对比:
// 水平触发: 保证了数据的完整输出, 只要有数据系统就会通知, 数据量大的情况下会
//           不断通知
// 边沿触发: 需要用户自己保证数据的完整输出, 数据到来时只会通知一次, 在数据量
//           大的情况下, 可以避免大量的 epoll_wait
//
// 边沿触发需要配合非阻塞 I/O
// 阻塞 IO + 边沿触发, 如果没数据可读, recv 会阻塞, 导致程序无法继续