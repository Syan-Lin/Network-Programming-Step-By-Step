#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <error.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include "../dbg.h"

using namespace std;

// 编译命令 g++ server.cpp -o server

//                                              +---------------------------------------+
//                                              |                                       |
//                                              v                                       |
// 服务端流程 [创建 socket] -> [绑定地址] -> [监听连接] -> [接收数据] -> [发送数据] -> [关闭连接] -> [退出]
//                                                           |                          ^
//                                                           |                          |
//                                                           +----------if(Q)-----------+

#define MAX_BUFF 1024
#define EPOLL_SIZE 64

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
        // 设置协议、监听端口和地址
        addr_serv.sin_family = AF_INET;					// AF_INET:    IPv4
        addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY: 监听所有 IP 地址
        addr_serv.sin_port = htons(port);				// htons:      本地序 -> 网络序

        // 创建套接字，参数(协议族，传输方式，协议)
        socket_serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(socket_serv == -1) {
            error_handler(SOCKET_ERROR);
            return;
        }

        // 将地址绑定至套接字
        if(bind(socket_serv, (struct sockaddr*)&addr_serv, sizeof(addr_serv)) == -1) {
            error_handler(BIND_ERROR);
            return;
        }

        // 跳过 Time_wait 这一阶段, 使服务器可以立即重启
        socklen_t temp = sizeof(socket_serv);
        setsockopt(socket_serv, SOL_SOCKET, SO_REUSEADDR, nullptr, temp);

        // epoll 相关
        epfd = epoll_create(EPOLL_SIZE); // Linux 内核版本大于 2.6.8 该参数没有意义
        ep_events = (epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = socket_serv;
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_serv, &event); // 添加服务端套接字监听
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
            int event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
            if(event_cnt == -1) {
                error_handler(EPOLL_ERROR);
                return;
            } else if(event_cnt == 0) {
                continue;
            }

            for(int i = 0; i < event_cnt; i++) {
                if(ep_events[i].data.fd == socket_serv) {
                    add_socket();
                } else {
                    int sock = ep_events[i].data.fd;
                    thread work_thread(&Server::business, this, sock);
                    work_thread.detach();
                }
            }
        }
    }

    void shutdown() { // 关闭套接字
        if(error == SOCKET_ERROR) return;
        close(socket_serv);
        close(epfd);
    }

    void set_epoll_et(bool val) {
        edge_trigger = val;
    }

private:
    struct sockaddr_in addr_serv;
    struct sockaddr_in addr_clnt;
    int socket_serv;
    int socket_clnt;
    socklen_t len;
    vector<int> socks;

    // epoll 相关参数
    struct epoll_event* ep_events; // 储存所有产生的事件
    bool edge_trigger = false;     // epoll 默认为水平触发
    int epfd;

    // 多线程相关
    mutex loc;

    void business(int sock) {
        dbg(this_thread::get_id());
        dbg(socks);
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
                        cout << "[socket " << sock << "](" << times << " times): ";
                        break;
                    } else {
                        error_handler(RECV_ERROR);
                        return;
                    }
                }
                memcpy((char*)raw_data + begin, temp, rtn_val);
                begin += rtn_val;
            }
        } else { // 水平触发 + 阻塞 IO, 不用循环
            rtn_val = recv(sock, temp, sizeof(temp), 0);
            times++;
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
                return;
            }
            memcpy(raw_data, temp, rtn_val);
        }

        string data(raw_data);
        cout << data << endl;

        unique_lock ul(loc);

        if(data == "Quit" || rtn_val == 0) { // 断开连接或主动退出
            // 注销套接字
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            cout << "[socket " << sock << "] disconnect" << endl;
            for(int i = 0; i < socks.size(); i++) {
                if(socks[i] == sock) {
                    socks.erase(socks.begin() + i, socks.begin() + i + 1);
                }
            }
        } else {
            // 发送数据
            for(int i = 0; i < socks.size(); i++) {
                if(socks[i] != sock) {
                    if(send(socks[i], data.c_str(), data.size(), 0) == -1) {
                        error_handler(SEND_ERROR);
                    }
                }
            }
        }
    }

    void add_socket() {
        // 连接请求通过数据传输完成, 服务端套接字有数据需要读, 代表连接请求
        // 获取来自客户端的连接，如果没有则会阻塞
        socket_clnt = accept(socket_serv, nullptr, nullptr);
        if(socket_clnt == -1) {
            error_handler(SOCKET_ERROR);
            return;
        }

        struct epoll_event event;
        if(edge_trigger) { // 边沿触发
            set_no_block(socket_clnt);
            event.events = EPOLLIN | EPOLLET;
        } else {
            event.events = EPOLLIN;
        }

        event.data.fd = socket_clnt;

        unique_lock ul(loc);
        socks.push_back(socket_clnt);
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_clnt, &event); // 添加客户端套接字监听

        cout << "[socket " << socket_clnt << "] connect" << endl;
    }

    enum ERROR { NUll = 0, SOCKET_ERROR, BIND_ERROR, LISTEN_ERROR, SEND_ERROR, RECV_ERROR, EPOLL_ERROR };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR: cout << "socket error" << endl; break;
            case BIND_ERROR:   cout << "bind error" << endl;   break;
            case LISTEN_ERROR: cout << "listen error" << endl; break;
            case SEND_ERROR:   cout << "send error" << endl;   break;
            case RECV_ERROR:   cout << "recv error" << endl;   break;
            case EPOLL_ERROR:  cout << "epoll error" << endl;  break;
        }
    }
    void set_no_block(int fd) {
        int flag = fcntl(fd, F_GETFL, 0); // 得到套接字原来属性
        fcntl(fd, F_SETFL, flag | O_NONBLOCK);// 在原有属性基础上设置添加非阻塞模式
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