#pragma once
#include <wfrest/HttpServer.h>
#include <workflow/WFFacilities.h>

// 面向对象：将专业的事情交给专业的“人”去做
// 高内聚，低耦合

// 设计原则：武学心法
// 设计模式：招数

// 设计原则：组合优于继承
//     组合：有选择的复用代码
//     继承：会复用基类的所有代码，代码复用的技术！

// 装饰器模式（Wrapper）: 组合优于继承 (代码的复用)
// 继承：复用所有代码
// 组合：有选择的复用的代码
// 注意事项：保持接口一致，可以降低用户的学习成本
class CloudDiskServer {
public:
    CloudDiskServer() { }

    // 注册路由
    void register_routes();

    // 包装了一层: 要保证包装后的接口与原来的接口一致！
    // 接口一致：降低用户的学习成本!
    int start(unsigned short port)
    {
        return server_.start(port);
    }

    void stop() { server_.stop(); }

    void list_routes() { server_.list_routes(); }

private:
    // 注册路由
    void register_www_module(); // 静态资源模块
    void register_auth_module(); // 登录注册
    void register_user_module(); // 用户模块
    void register_file_module(); // 文件模块

private:
    // 名字中最好不要带具体的实现细节
    // 方便以后修改具体的实现
    wfrest::HttpServer server_; // 组合
};
