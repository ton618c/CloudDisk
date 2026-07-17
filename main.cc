#include <signal.h>

#include <fstream>
#include <iostream>

#include "CloudDiskServer.h"
#include "OssManager.hh"
using namespace std;

WFFacilities::WaitGroup waitGroup(1);

void sig_handler(int) { waitGroup.done(); }

// 读取 .env 文件，将 KEY=VALUE 设为环境变量
void load_env(const string& path) {
    ifstream ifs(path);
    if (!ifs) {
        cerr << "fail to open .env" << endl;
        return;
    }
    string line;
    while (getline(ifs, line)) {
        auto eq = line.find('=');
        string key = line.substr(0, eq);
        string val = line.substr(eq + 1);
        setenv(key.c_str(), val.c_str(), 1);
    }
}

int main() {
    signal(SIGINT, sig_handler);
    srand(time(NULL));
    load_env(".env");  // 加载 .env 文件 取ID和KEY

    CloudDiskServer server;
    AutoRelease ar{OssManager::getInstance()};
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
