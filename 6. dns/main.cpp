#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include "../dbg.h"

using namespace std;

int main() {
    // 利用域名获取 IP 等相关信息
    cout << "=========get info by name=========" << endl;
    hostent* host = gethostbyname("www.baidu.com");
    if(host == nullptr) {
        cout << "failed!" << endl;
        return 0;
    }
    cout << "hostname: " << host->h_name << endl;
    for(int i = 0; host->h_aliases[i]; i++) {
        cout << "aliase " << i << ": " << host->h_aliases[i] << endl;
    }
    string af = (host->h_addrtype == AF_INET) ? "IPv4" : "IPv6";
    cout << "protocol: " << af << endl;
    char* ip;
    for(int i = 0; host->h_addr_list[i]; i++) {
        ip = inet_ntoa(*(struct in_addr*)host->h_addr_list[i]);
        cout << "ip " << i << ": " << ip << endl;
    }

    // 利用 IP 获取域名等相关信息
    cout << "=========get info by ip=========" << endl;
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr(ip);

    hostent* host2 = gethostbyaddr((char*)&addr.sin_addr, 4, AF_INET);     // IPv4 第二个参数是 4, IPv6 是 16
    if(host2 == nullptr) {
        cout << "failed!" << endl;
        return 0;
    }
    cout << "hostname: " << host2->h_name << endl;
    for(int i = 0; host2->h_aliases[i]; i++) {
        cout << "aliase " << i << ": " << host2->h_aliases[i] << endl;
    }
    af = (host2->h_addrtype == AF_INET) ? "IPv4" : "IPv6";
    cout << "protocol: " << af << endl;
    for(int i = 0; host2->h_addr_list[i]; i++) {
        ip = inet_ntoa(*(struct in_addr*)host2->h_addr_list[i]);
        cout << "ip " << i << ": " << ip << endl;
    }

    return 0;
}