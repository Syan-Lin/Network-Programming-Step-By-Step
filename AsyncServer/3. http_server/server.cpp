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
#include <fstream>
#include "../../dbg.h"

using namespace std;

#define MAX_BUFF 1024
#define EPOLL_SIZE 64

class HttpServer {
public:
    HttpServer() {
        memset(&addr_serv, 0, sizeof(addr_serv));
    }
    HttpServer(const HttpServer&)            = delete;
    HttpServer(HttpServer&&)                 = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer& operator=(HttpServer&&)      = delete;

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

        // epoll 相关
        epfd = epoll_create(EPOLL_SIZE); // Linux 内核版本大于 2.6.8 该参数没有意义
        ep_events = (epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = socket_serv;
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_serv, &event); // 监听服务端套接字
    }

    void run() {
        if(error > 0) return;
        // 开始监听
        if(listen(socket_serv, 8) == -1) {
            error_handler(LISTEN_ERROR);
            return;
        }
        cout << "HttpServer is running!" << endl;
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
                    thread work_thread(&HttpServer::business, this, sock);
                    work_thread.detach();
                }
            }
        }
    }

    void shutdown() {
        if(error == SOCKET_ERROR) return;
        close(socket_serv);
        close(epfd);
    }

    void set_epoll_et(bool val) {
        edge_trigger = val;
    }

private:
    // HTTP 协议不是重点, 这里仅读取第一行内容
    // 判断命令 GET, 获取对应的文件, 仅此而已
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
                        // cout << "[socket " << sock << "](" << times << " times): ";
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

        // 报文处理
        string data(raw_data);
        if(data.size() < 6) {
            send_error(400, sock);
            unique_lock<mutex> ul(loc);
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            return;
        }
        dbg(data);

        string method = data.substr(0, 3);
        dbg(method);
        if(method != "GET") {
            send_error(400, sock);
            unique_lock<mutex> ul(loc);
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            return;
        }

        string filename;
        for(int i = 5; i < data.size(); i++) {
            if(data[i] != ' ') {
                filename += data[i];
            } else {
                break;
            }
        }
        dbg(filename);
        ifstream file(filename, ios::in | ios::binary);
        if(!file) {
            send_error(404, sock);
            unique_lock<mutex> ul(loc);
            epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
            close(sock);
            return;
        }

        // 读取文件
        file.seekg(0, std::ios_base::end);
        int file_size = file.tellg();
        file.seekg(0, std::ios_base::beg);

        char* file_raw = new char[file_size];
        file.read(file_raw, file_size);
        file.close();

        string message = "HTTP/1.0 200 OK\r\n\
                          Server:Linux Http Server\r\n\
                          Content-length: 2048\r\n\
                          Content-type:text/plain\r\n\r\n";
        string body(file_raw);
        message += body;

        dbg(message);

        // 发送数据
        if(send(sock, message.c_str(), message.size(), 0) == -1) {
            error_handler(SEND_ERROR);
        }
        
        unique_lock<mutex> ul(loc);
        epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
        close(sock);
        delete file_raw;
    }

    void send_error(int code, int sock) {
        dbg(code);
        string message;
        if(code == 404) {
            message = "HTTP/1.0 404 Not Found\r\n";
        } else if(code == 400) {
            message = "HTTP/1.0 400 Bad Request\r\n";
        }
        message += "Server:Linux Http Server\r\n\
                    Content-length:2048\r\n\
                    Content-type:text/html\r\n\r\n\
                    <html><head><title>error</title><head>\
                    <body>Error!</body></html>";
        // 发送数据
        if(send(sock, message.c_str(), message.size(), 0) == -1) {
            error_handler(SEND_ERROR);
        }
    }

    void add_socket() {
        // 连接请求通过数据传输完成, 服务端套接字有数据需要读, 代表连接请求
        // 获取来自客户端的连接，如果没有则会阻塞
        int socket_clnt = accept(socket_serv, nullptr, nullptr);
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
        epoll_ctl(epfd, EPOLL_CTL_ADD, socket_clnt, &event); // 添加客户端套接字监听

        cout << "[socket " << socket_clnt << "] connect" << endl;
    }

private:
    struct sockaddr_in addr_serv;
    int socket_serv;

    // epoll 相关参数
    struct epoll_event* ep_events; // 储存所有产生的事件
    bool edge_trigger = false;     // epoll 默认为水平触发
    int epfd;

    // 多线程相关
    mutex loc;

private:
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
        int flag = fcntl(fd, F_GETFL, 0);     // 得到套接字原来属性
        fcntl(fd, F_SETFL, flag | O_NONBLOCK);// 在原有属性基础上设置添加非阻塞模式
    }
};

int main() {
    HttpServer server;
    server.set_epoll_et(true); // 边沿触发
    server.init(8080);
    server.run();
    server.shutdown();

    return 0;
}
