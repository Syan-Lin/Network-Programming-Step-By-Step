#pragma once
#include <iostream>

enum class ERROR { NO_ERROR, SOCKET_ERROR, BIND_ERROR, LISTEN_ERROR, SEND_ERROR,
                    RECV_ERROR, EPOLL_ERROR, SELECT_ERROR, CONNECT_ERROR,
                    DISCONNECT, SIGNAL };

class ErrorHandler {
public:
    static void handle(ERROR code) {
        using std::cout;
        using std::endl;
        error = code;
        switch(code) {
            case ERROR::SOCKET_ERROR:  cout << "socket error" << endl;  break;
            case ERROR::CONNECT_ERROR: cout << "connect error" << endl; break;
            case ERROR::SEND_ERROR:    cout << "send error" << endl;    break;
            case ERROR::RECV_ERROR:    cout << "recv error" << endl;    break;
            case ERROR::BIND_ERROR:    cout << "bind error" << endl;    break;
            case ERROR::LISTEN_ERROR:  cout << "listen error" << endl;  break;
            case ERROR::EPOLL_ERROR:   cout << "epoll error" << endl;   break;
            case ERROR::SELECT_ERROR:  cout << "select error" << endl;  break;
            case ERROR::DISCONNECT:    cout << "disconnect" << endl;    break;
            case ERROR::SIGNAL:        cout << "signal error" << endl;  break;
            case ERROR::NO_ERROR: break;
        }
    }
    static ERROR error;
};

inline ERROR ErrorHandler::error = ERROR::NO_ERROR;