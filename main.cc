#include <signal.h>

#include <fstream>
#include <iostream>

#include "CloudDiskServer.h"
#include "OssManager.hh"
using namespace std;

WFFacilities::WaitGroup waitGroup(1);

void sig_handler(int) { waitGroup.done(); }

int main() {
    signal(SIGINT, sig_handler);
    srand(time(NULL));

    CloudDiskServer server;
    // AutoRelease ar{OssManager::getInstance()};
    // 注册路由
    server.register_routes();

    if (server.start(8888) == 0) {
        server.list_routes();
        waitGroup.wait();
        server.stop();
    } else {
        cerr << "Error: Server start FAILED!" << endl;
    }
}
