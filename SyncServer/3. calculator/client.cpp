#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

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
            error_handler(SOCKET_ERROR);
            return;
        }
    }

    void run() {
        if(error > 0) return;
        // 连接服务器
        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            error_handler(CONNECT_ERROR);
            return;
        }
        cout << "Connect to server" << endl;

        // 业务
        business();
    }

    void shutdown() {
        if(error == SOCKET_ERROR) return;
        close(sock);
    }

private:
    void business() {
        while(true) {
            // 输入数据
            int count;
            cout << "Operand count: ";
            cin >> count;
            if(count <= 0) { break; }

            vector<int> data(count);
            for(int i = 0; i < count; i++) {
                cout << "Operand " << i + 1 << ": ";
                cin >> data[i];
            }

            char opt;
            cout << "Operator: ";
            cin >> opt;

            // 数据格式: |int|int...(count)|char| 传输给服务端 count + 1 个 int 和 1 个 char
            int size = 4 * (count + 1) + 1; // 字节数
            char* bytes = new char[size];
            *(int*)bytes = count; // 设置第一个 int
            for(int i = 0; i < count; i++){ // 设置 count 个 int
                *((int*)bytes + i + 1) = data[i];
            }
            bytes[size - 1] = opt; // 设置最后一个 char

            // 发送数据
            if(send(sock, bytes, size, 0) == -1) {
                error_handler(SEND_ERROR);
                delete bytes;
                continue;
            }
            delete bytes;

            // 接收数据
            int result;
            int rtn_val = recv(sock, &result, sizeof(result), 0);
            if(rtn_val == -1) {
                error_handler(RECV_ERROR);
            } else if(rtn_val == 0) { // 服务器断开连接
                error_handler(DISCONNECT);
                break;
            }
            cout << "Result: " << result << endl;
        }
    }

private:
    struct sockaddr_in addr;
    int sock;

private:
    enum ERROR { NUll = 0, SOCKET_ERROR, CONNECT_ERROR, SEND_ERROR, RECV_ERROR, DISCONNECT };
    ERROR error = NUll;
    void error_handler(ERROR code) {
        error = code;
        switch(code) {
            case SOCKET_ERROR:  cout << "socket error" << endl;      break;
            case CONNECT_ERROR: cout << "bind error" << endl;        break;
            case SEND_ERROR:    cout << "send error" << endl;        break;
            case RECV_ERROR:    cout << "recv error" << endl;        break;
            case DISCONNECT:    cout << "server disconnect" << endl; break;
        }
    }
};

int main() {
    Client client;
    client.init("127.0.0.1", 6666);
    client.run();
    client.shutdown();

    return 0;
}