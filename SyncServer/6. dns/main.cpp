#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>

using namespace std;

int main() {
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

    return 0;
}