#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <thread>
#include <string.h>

using namespace std;
int sockyy = socket(PF_INET, SOCK_STREAM, 0);


void inputService(bool *run) {
    string input;
    while (*run) {
        cin >> input;
        if (input == "1") {
            *run = false;
        }
            cout<<input.c_str()<<endl;
            write(sockyy, input.c_str(), input.size());
    }
}


int main(int argc, char **argv) {
    uint16_t host = atoi(argv[2]);
    uint16_t network = htons(host);

    sockaddr_in zmienna;
    zmienna.sin_family = AF_INET;
    zmienna.sin_port = network;
    zmienna.sin_addr.s_addr = inet_addr(argv[1]);
    int result = connect(sockyy, (sockaddr *) &zmienna, 30);
    bool run = true;
    thread inputServiceThread(inputService, &run);
    char msg[100];
    int msgsize;
    ssize_t readBytes;
    while (run) {
        readBytes = read(sockyy, msg, 100);
        write(1, msg, readBytes);
        cout<<endl;
    }
    inputServiceThread.join();
    close(sockyy);
    return 0;
}