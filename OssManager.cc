#include "OssManager.hh"

#include <alibabacloud/oss/OssClient.h>
OssManager* OssManager::_ossInstance = new OssManager{};
using namespace AlibabaCloud::OSS;
OssManager::OssManager() {
    cout << "初始化网络等资源" << endl;
    InitializeSdk();
}

OssManager::~OssManager() {
    cout << "释放网络等资源" << endl;
    ShutdownSdk();
}

OssManager* OssManager::getInstance() {  // double check
    // if (!_ossInstance) {
    //     _ossInstance = new OssManager{};
    // }
    return _ossInstance;
}

void OssManager::upload_file_to_oss(string object_name, string content) {
    // 2. 设置OSS账号信息，创建OssClient
    string endpoint = "oss-cn-wuhan-lr.aliyuncs.com";
    const char* aki = getenv("OSS_ACCESS_KEY_ID");
    const char* aks = getenv("OSS_ACCESS_KEY_SECRET");
    if (!aki || !aks) {
        cerr << "请在.env文件中设置id和key值" << endl;
        return;
    }
    string accessKeyId = aki;
    string accessKeySecret = aks;
    string region = "cn-wuhan";
    ClientConfiguration conf;
    OssClient client(endpoint, accessKeyId, accessKeySecret, conf);
    client.SetRegion(region);
    // 3. 上传文件
    string bucketName = "clouddisk-storage";
    string objectName = object_name;
    shared_ptr<iostream> stream = make_shared<stringstream>(move(content));
    PutObjectRequest request(bucketName, objectName, stream);
    auto outcome = client.PutObject(request);
    // 4. 错误处理
    if (!outcome.isSuccess()) {
        cout << "PutObject FAILED"
             << ", code:" << outcome.error().Code() << ", message:" << outcome.error().Message()
             << ", requestId:" << outcome.error().RequestId() << endl;
        exit(1);
    }
}