#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "OssManager.hh"

using namespace std;
using namespace AmqpClient;
using json = nlohmann::json;

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
    // 消费者是独立进程，必须自己加载 .env
    load_env(".env");
    AutoRelease ar{OssManager::getInstance()};

    // 连接 RabbitMQ
    string uri = "amqp://guest:guest@localhost:5672/%2f";
    Channel::ptr_t channel = Channel::CreateFromUri(uri);

    const string queue = "oss.queue";
    channel->BasicConsume(queue);

    cout << "============启动消费者成功============" << endl;

    while (true) {
        Envelope::ptr_t envelope = channel->BasicConsumeMessage();
        if (envelope && envelope->Message()) {
            string body = envelope->Message()->Body();
            cout << "[RabbitConsumer] 收到任务: " << body << endl;
            json task = json::parse(body);
            string path = task["path"];
            string objectName = task["objectName"];
            ifstream ifs(path, ios::binary);
            if (!ifs) {
                cerr << "fail open file: " << path << endl;
                continue;
            }
            ostringstream oss;
            oss << ifs.rdbuf();
            string content = oss.str();
            bool flag = OssManager::getInstance()->upload_file_to_oss(objectName, content);
            if (flag) {
                cout << "任务：" << body << "--执行成功" << endl;
            } else {
                cout << "任务：" << body << "--执行失败" << endl;
            }
        }
    }

    return 0;
}
