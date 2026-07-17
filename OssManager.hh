#pragma once

#include <alibabacloud/oss/OssClient.h>

#include <cstddef>
#include <string>
using namespace std;

class OssManager {
public:
    static OssManager* getInstance();
    void upload_file_to_oss(string object_name, string file);
    OssManager(const OssManager& rhs) = delete;
    OssManager& operator=(const OssManager& rhs) = delete;

private:
    OssManager();
    ~OssManager();
    static OssManager* _ossInstance;
    friend class AutoRelease;
};

class AutoRelease {
public:
    AutoRelease(OssManager* om) : _om(om) { cout << "AutoRelease(OssManager)" << endl; }
    ~AutoRelease() {
        cout << "~AutoRelease()" << endl;
        if (_om) {
            delete _om;
            _om = nullptr;
        }
    }

private:
    OssManager* _om;
};