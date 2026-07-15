#include "CloudDiskServer.h"
#include "CryptoUtil.h"
#include "common.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <wfrest/PathUtil.h>
#include <workflow/HttpUtil.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>
#include <workflow/mysql_types.h>

using namespace std;
using namespace std::placeholders;
using namespace wfrest;
using namespace protocol;
using json = nlohmann::json;

// 数据库的URL（需要修改！）
static const string DatabaseURL = "mysql://root:1234@localhost/CloudDisk";
static const int RetryMax = 3;

void CloudDiskServer::register_routes()
{
    // 设置静态资源的路由
    register_www_module();
    register_auth_module();
    // register_user_module();
    // register_file_module();
    // ...
}

void CloudDiskServer::register_www_module()
{
    server_.Static("/", "./www/index.html");
    server_.Static("/static", "./www/static");
}

void CloudDiskServer::register_auth_module()
{
    // wfrest支持类型的处理函数：Handler, SeriesHandler(和Workflow集成)
    server_.POST("/api/v1/auth/register", [](const HttpReq* req, HttpResp* resp) {
        // 解析请求 (抓包)
        // 处理业务逻辑
        // 生成响应
    });

    server_.POST("/api/v1/auth/login", [](const HttpReq* req, HttpResp* resp, SeriesWork* series) {
        // 解析请求 (抓包)
        // 处理业务逻辑
        // 生成响应
    });
}
