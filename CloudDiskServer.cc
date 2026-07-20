#include "CloudDiskServer.h"

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <srpc/rpc_context.h>
#include <wfrest/HttpDef.h>
#include <wfrest/HttpMsg.h>
#include <wfrest/PathUtil.h>
#include <workflow/HttpUtil.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>
#include <workflow/mysql_types.h>

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

#include "CryptoUtil.h"
#include "OssManager.hh"
#include "auth.pb.h"
#include "auth.srpc.h"
#include "common.h"

using namespace std;
using namespace std::placeholders;
using namespace wfrest;
using namespace protocol;
using namespace AmqpClient;
using json = nlohmann::json;

// 数据库的URL
static const string DatabaseURL = "mysql://root:123456@localhost/CloudDisk";
static const int RetryMax = 3;

void CloudDiskServer::register_routes() {
    // 设置静态资源的路由
    register_www_module();
    register_auth_module();
    register_user_module();
    register_file_module();
    // ...
}

void CloudDiskServer::register_www_module() {
    server_.Static("/", "./www/index.html");
    server_.Static("/static", "./www/static");
}

void CloudDiskServer::register_auth_module() {
    // 将注册登录改造成微服务，此时可以将API网关看为客户端
    server_.POST(
        "/api/v1/auth/register", [](const HttpReq* req, HttpResp* resp, SeriesWork* serieswork) {
            Auth::SRPCClient auth_client("127.0.0.1", 1314);
            if (req->content_type() != wfrest::APPLICATION_JSON) {
                resp->set_status(HttpStatusBadRequest);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "请求格式有误";
                resp->Json(ret.dump());
                return;
            }
            json js = json::parse(req->body());
            RegisterRequest reg_req;
            reg_req.set_username(js["username"]);
            reg_req.set_password(js["password"]);
            reg_req.set_confirm(js["confirm"]);
            auto* task = auth_client.create_AuthRegister_task(
                [resp](RegisterResponse* response, srpc::RPCContext* context) {
                    if (!context->success()) {
                        cerr << "error code: " << context->get_error()
                             << ", error msg: " << context->get_errmsg() << endl;
                        resp->set_status(503);
                        json ret;
                        ret["status"] = "error";
                        ret["message"] = "服务暂时不可用";
                        resp->Json(ret.dump());
                        return;
                    }
                    resp->set_status(response->code());
                    json ret;
                    ret["status"] = response->status();
                    ret["message"] = response->message();
                    if (ret["message"] == "注册成功") {
                        ret["data"]["userId"] = response->userid();
                        ret["data"]["username"] = response->username();
                    }
                    resp->Json(ret.dump());
                });
            task->serialize_input(&reg_req);
            serieswork->push_back(task);
        });

    // wfrest支持类型的处理函数：Handler, SeriesHandler(和Workflow集成)
    // 注册逻辑
    // server_.POST("/api/v1/auth/register", [](const HttpReq* req, HttpResp* resp) {
    //     // 解析请求 (抓包)
    //     // 处理业务逻辑
    //     if (req->content_type() != wfrest::APPLICATION_JSON) {
    //         resp->set_status(HttpStatusBadRequest);
    //         json ret = json::object();
    //         ret["status"] = "error";
    //         ret["message"] = "请求格式有误";
    //         resp->Json(ret.dump());
    //         return;
    //     }
    //     json js = json::parse(req->body());
    //     string username = js["username"];
    //     string password = js["password"];
    //     string confirm = js["confirm"];
    //     if (username.empty() || password.empty()) {
    //         resp->set_status(HttpStatusBadRequest);
    //         json ret = json::object();
    //         ret["status"] = "error";
    //         ret["message"] = "用户名和密码不能为空";
    //         resp->Json(ret.dump());
    //         return;
    //     }
    //     if (!(password == confirm)) {
    //         resp->set_status(HttpStatusBadRequest);
    //         json ret = json::object();
    //         ret["status"] = "error";
    //         ret["message"] = "两次输入的密码不一致";
    //         resp->Json(ret.dump());
    //         return;
    //     }
    //     // 生成响应
    //     string pwhash = CryptoUtil::hash_password(password);
    //     string sql = "INSERT into tbl_user (username , pwhash) values ('" + username + "' , '" +
    //                  pwhash + "');";
    //     resp->MySQL(DatabaseURL, sql, [resp, username](MySQLResultCursor* cursor) {
    //         if (cursor->get_cursor_status() == MYSQL_STATUS_OK &&
    //             cursor->get_affected_rows() == 1) {
    //             resp->set_status(201);
    //             resp->add_header_pair("application", "json");
    //             json ret = json::object();
    //             int id = cursor->get_insert_id();
    //             ret["status"] = "success";
    //             ret["message"] = "注册成功";
    //             ret["data"]["userId"] = id;
    //             ret["data"]["username"] = username;
    //             resp->Json(ret.dump());
    //             filesystem::create_directories("upload_files/" + username);
    //             return;
    //         } else {
    //             resp->set_status(409);
    //             json ret = json::object();
    //             ret["status"] = "error";
    //             ret["message"] = "用户名已存在";
    //             resp->Json(ret.dump());
    //             return;
    //         }
    //     });
    // });
    // 将登录改造成微服务
    server_.POST("/api/v1/auth/login", [](const HttpReq* req, HttpResp* resp, SeriesWork* series) {
        Auth::SRPCClient auth_client("127.0.0.1", 1314);
        if (req->content_type() != wfrest::APPLICATION_JSON) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "请求格式有误";
            resp->Json(ret.dump());
            return;
        }
        json js = json::parse(req->body());
        LoginRequest lg_req;
        lg_req.set_username(js["username"]);
        lg_req.set_password(js["password"]);
        auto* task = auth_client.create_AuthLogin_task(
            [resp](LoginResponse* response, srpc::RPCContext* context) {
                if (!context->success()) {
                    cerr << "error code: " << context->get_error()
                         << ", error msg: " << context->get_errmsg() << endl;
                    resp->set_status(503);
                    json ret;
                    ret["status"] = "error";
                    ret["message"] = "服务暂时不可用";
                    resp->Json(ret.dump());
                    return;
                }
                resp->set_status(response->code());
                json ret;
                ret["status"] = response->status();
                ret["message"] = response->message();
                if (ret["message"] == "登录成功") {
                    ret["data"]["accessToken"] = response->accesstoken();
                    ret["data"]["tokenType"] = response->tokentype();
                    ret["data"]["user"]["userId"] = response->userid();
                    ret["data"]["user"]["username"] = response->username();
                }
                resp->Json(ret.dump());
            });
        // 在调用http任务之前 根据rpc的返回结果
        // 来填充resp
        task->serialize_input(&lg_req);
        series->push_back(task);
    });
    //登录逻辑
    // server_.POST("/api/v1/auth/login", [](const HttpReq* req, HttpResp* resp, SeriesWork* series)
    // {
    //     // 解析请求 (抓包)
    //     // 处理业务逻辑
    //     if (req->content_type() != wfrest::APPLICATION_JSON) {
    //         resp->set_status(HttpStatusBadRequest);
    //         json ret = json::object();
    //         ret["status"] = "error";
    //         ret["message"] = "请求格式有误";
    //         resp->Json(ret.dump());
    //         return;
    //     }
    //     json js = json::parse(req->body());
    //     string username = js["username"];
    //     string password = js["password"];
    //     if (username.empty() || password.empty()) {
    //         resp->set_status(HttpStatusBadRequest);
    //         json ret = json::object();
    //         ret["status"] = "error";
    //         ret["message"] = "用户名和密码不能为空";
    //         resp->Json(ret.dump());
    //         return;
    //     }
    //     string sql = "SELECT * from tbl_user WHERE username='" + username + "';";
    //     cout << "[sql] : " << sql << endl;
    //     resp->MySQL(DatabaseURL, sql, [resp, password](MySQLResultCursor* cursor) {
    //         if (cursor->get_cursor_status() != MYSQL_STATUS_GET_RESULT) {
    //             resp->set_status(500);
    //             json ret = json::object();
    //             ret["status"] = "error";
    //             ret["message"] = "内部服务器错误";
    //             resp->Json(ret.dump());
    //             return;
    //         }
    //         if (cursor->get_rows_count() == 0) {
    //             resp->set_status(401);
    //             json ret = json::object();
    //             ret["status"] = "error";
    //             ret["message"] = "用户名或密码错误";
    //             resp->Json(ret.dump());
    //             return;
    //         }
    //         User user;
    //         map<string, MySQLCell> record;
    //         cursor->fetch_row(record);
    //         user.id = record["id"].as_int();
    //         user.pwhash = record["pwhash"].as_string();
    //         user.username = record["username"].as_string();
    //         user.createdAt = record["created_at"].as_string();
    //         if (CryptoUtil::verify_password(password, user.pwhash)) {
    //             string token = CryptoUtil::generate_token(user);
    //             resp->set_status(200);
    //             resp->add_header_pair("application", "json");
    //             json ret = json::object();
    //             ret["status"] = "success";
    //             ret["message"] = "登录成功";
    //             ret["data"]["accessToken"] = token;
    //             ret["data"]["tokenType"] = "Bearer";
    //             ret["data"]["user"]["userId"] = user.id;
    //             ret["data"]["user"]["username"] = user.username;
    //             resp->Json(ret.dump());
    //             return;
    //         } else {
    //             resp->set_status(401);
    //             json ret = json::object();
    //             ret["status"] = "error";
    //             ret["message"] = "用户名或密码错误";
    //             resp->Json(ret.dump());
    //             return;
    //         }
    //     });
    //     // 生成响应
    // });
}

// 获取用户信息
void CloudDiskServer::register_user_module() {
    server_.GET("/api/v1/user/me", [](const HttpReq* req, HttpResp* resp) {
        string authorization = req->header("Authorization");
        if (authorization.empty() || authorization.find("Bearer ") != 0) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        User user;
        string token = authorization.substr(7);
        if (!CryptoUtil::verify_token(token, user)) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        } else {
            // token验证成功后 user只会存储id和username
            // 要获得created_at只能通过sql查询
            string sql =
                "SELECT id, username, created_at FROM tbl_user WHERE id=" + to_string(user.id);
            resp->MySQL(DatabaseURL, sql, [resp](MySQLResultCursor* cursor) {
                // 拿到完整的、最新的用户信息
                if (cursor->get_cursor_status() != MYSQL_STATUS_GET_RESULT) {
                    resp->set_status(500);
                    json ret = json::object();
                    ret["status"] = "error";
                    ret["message"] = "内部服务器错误";
                    resp->Json(ret.dump());
                    return;
                }
                User user;
                map<string, MySQLCell> record;
                cursor->fetch_row(record);
                user.id = record["id"].as_int();
                user.username = record["username"].as_string();
                user.createdAt = record["created_at"].as_string();
                resp->set_status(200);
                resp->add_header_pair("application", "json");
                json ret = json::object();
                ret["status"] = "success";
                ret["message"] = "获取个人信息成功";
                ret["data"]["userId"] = user.id;
                ret["data"]["username"] = user.username;
                ret["data"]["createdAt"] = user.createdAt;
                resp->Json(ret.dump());
                return;
            });
        }
    });
}

// 获取文件列表
void CloudDiskServer::register_file_module() {
    server_.GET("/api/v1/files", [](const HttpReq* req, HttpResp* resp) {
        string authorization = req->header("Authorization");
        if (authorization.empty() || authorization.find("Bearer ") != 0) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        User user;
        string token = authorization.substr(7);
        if (!CryptoUtil::verify_token(token, user)) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        // 令牌校验成功 返回文件列表
        // 文件列表可以通过uid 查询
        string sql = "select * from tbl_file where uid = " + to_string(user.id) + ";";
        cout << "[sql] : " << sql << endl;
        resp->MySQL(DatabaseURL, sql, [resp](MySQLResultCursor* cursor) {
            if (cursor->get_cursor_status() != MYSQL_STATUS_GET_RESULT) {
                resp->set_status(500);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "内部服务器错误";
                resp->Json(ret.dump());
                return;
            }
            resp->set_status(200);
            resp->add_header_pair("application", "json");
            // 此错有可能解析错误
            json ret = json::object();
            ret["status"] = "success";
            ret["message"] = "获取文件列表成功";
            json files = json::array();
            map<string, MySQLCell> record;
            while (cursor->fetch_row(record)) {
                json file = json::object();
                file["fileId"] = record["id"].as_int();
                file["filename"] = record["filename"].as_string();
                file["size"] = record["size"].as_int();
                file["createdAt"] = record["created_at"].as_string();
                file["updatedAt"] = record["last_update"].as_string();
                files.push_back(file);
            }
            ret["data"]["files"] = files;
            resp->Json(ret.dump());
            return;
        });
    });

    // 上传文件
    server_.POST("/api/v1/files", [](const HttpReq* req, HttpResp* resp) {
        string authorization = req->header("Authorization");
        if (authorization.empty() || authorization.find("Bearer ") != 0) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        User user;
        string token = authorization.substr(7);
        if (!CryptoUtil::verify_token(token, user)) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        // user结构体里面现在有id 和 username
        if (req->content_type() != MULTIPART_FORM_DATA) {
            resp->set_status(400);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "请求格式有误";
            resp->Json(ret.dump());
            return;
        }
        const Form& form = req->form();
        for (const auto& [_, file] : form) {
            // 获取文件名和其文件内容
            const string& filename = file.first;
            const string& content = file.second;
            // 获得/的最后一个文件名， 为了安全起见
            string basename = PathUtil::base(filename);
            string hashcode = CryptoUtil::generate_hashcode(
                (const unsigned char*)content.c_str(), content.size());
            string sql = "INSERT INTO tbl_file (uid , filename , hashcode , size) VALUES(" +
                         to_string(user.id) + ", '" + basename + "', '" + hashcode + "', " +
                         to_string(content.size()) + ");";
            cout << "[sql] : " << sql << endl;
            resp->MySQL(
                DatabaseURL, sql, [user, basename, content, resp](MySQLResultCursor* cursor) {
                    if (cursor->get_cursor_status() == MYSQL_STATUS_OK &&
                        cursor->get_affected_rows() == 1) {
                        // 如果数据库任务执行成功 ，我们就给他存到本地
                        filesystem::create_directories("upload_files/" + user.username);
                        string path = "upload_files/" + user.username + "/" + basename;
                        // 这里不再直接对文件进行备份
                        // 而是通过channem上传到exchange
                        // 让消息队列异步执行
                        resp->Save(path, std::move(content), [path](const struct FileIOArgs*) {
                            json task;
                            task["path"] = path;
                            task["objectName"] = path;
                            auto channel =
                                Channel::Create("127.0.0.1", 5672, "guest", "guest", "/");
                            auto message = BasicMessage::Create(task.dump());
                            channel->BasicPublish("oss.direct", "oss", message);
                        });
                        resp->set_status(200);
                        resp->add_header_pair("application", "json");
                        json ret = json::object();
                        ret["status"] = "success";
                        ret["message"] = "上传成功";
                        ret["data"]["fileId"] = cursor->get_insert_id();
                        ret["data"]["filename"] = basename;
                        resp->Json(ret.dump());
                    } else {
                        // 这里虽然说的是内部服务器错误 ， 但其实filename相同也会出问题
                        resp->set_status(500);
                        json ret = json::object();
                        ret["status"] = "error";
                        ret["message"] = "内部服务器错误";
                        resp->Json(ret.dump());
                        return;
                    }
                });
        }
    });

    server_.GET("/api/v1/file/{id}", [](const HttpReq* req, HttpResp* resp) {
        string authorization = req->header("Authorization");
        if (authorization.empty() || authorization.find("Bearer ") != 0) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        User user;
        string token = authorization.substr(7);
        if (!CryptoUtil::verify_token(token, user)) {
            resp->set_status(401);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "无效的访问令牌";
            resp->Json(ret.dump());
            return;
        }
        const string& s_id = req->param("id");
        int id = stoi(s_id);
        string sql = "select filename from tbl_file where id = " + to_string(id) + ";";
        resp->MySQL(DatabaseURL, sql, [resp, user](MySQLResultCursor* cursor) {
            if (cursor->get_cursor_status() != MYSQL_STATUS_GET_RESULT) {
                resp->set_status(500);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "内部服务器错误";
                resp->Json(ret.dump());
                return;
            }
            if (cursor->get_rows_count() == 0) {
                resp->set_status(404);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "文件不存在";
                resp->Json(ret.dump());
                return;
            }
            resp->set_status(200);
            map<string, MySQLCell> record;
            cursor->fetch_row(record);
            resp->add_header_pair(
                "Content-Disposition", "attachment;filename=" + record["filename"].as_string());
            string path = "upload_files/" + user.username + "/" + record["filename"].as_string();
            resp->File(path);
        });
    });
}