#include "CloudDiskServer.h"

#include <wfrest/HttpDef.h>
#include <wfrest/PathUtil.h>
#include <workflow/HttpUtil.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>
#include <workflow/mysql_types.h>

#include <iostream>
#include <nlohmann/json.hpp>

#include "CryptoUtil.h"
#include "common.h"

using namespace std;
using namespace std::placeholders;
using namespace wfrest;
using namespace protocol;
using json = nlohmann::json;

// 数据库的URL（需要修改！）
static const string DatabaseURL = "mysql://root:123456@localhost/CloudDisk";
static const int RetryMax = 3;

void CloudDiskServer::register_routes() {
    // 设置静态资源的路由
    register_www_module();
    register_auth_module();
    // register_user_module();
    // register_file_module();
    // ...
}

void CloudDiskServer::register_www_module() {
    server_.Static("/", "./www/index.html");
    server_.Static("/static", "./www/static");
}

void CloudDiskServer::register_auth_module() {
    // wfrest支持类型的处理函数：Handler, SeriesHandler(和Workflow集成)
    server_.POST("/api/v1/auth/register", [](const HttpReq* req, HttpResp* resp) {
        // 解析请求 (抓包)
        // 处理业务逻辑
        if (req->content_type() != wfrest::APPLICATION_JSON) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "请求格式有误";
            resp->Json(ret.dump());
            return;
        }
        json js = json::parse(req->body());
        string username = js["username"];
        string password = js["password"];
        string confirm = js["confirm"];
        if (username.empty() || password.empty()) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "用户名和密码不能为空";
            resp->Json(ret.dump());
            return;
        }
        if (!(password == confirm)) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "两次输入的密码不一致";
            resp->Json(ret.dump());
            return;
        }
        // 生成响应
        string pwhash = CryptoUtil::hash_password(password);
        string sql = "INSERT into tbl_user (username , pwhash) values ('" + username + "' , '" +
                     pwhash + "');";
        resp->MySQL(DatabaseURL, sql, [resp, username](MySQLResultCursor* cursor) {
            if (cursor->get_cursor_status() == MYSQL_STATUS_OK &&
                cursor->get_affected_rows() == 1) {
                resp->set_status(201);
                resp->add_header_pair("application", "json");
                json ret = json::object();
                int id = cursor->get_insert_id();
                ret["status"] = "success";
                ret["message"] = "注册成功";
                ret["data"]["userId"] = id;
                ret["data"]["username"] = username;
                resp->Json(ret.dump());
                return;
            } else {
                resp->set_status(409);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "用户名已存在";
                resp->Json(ret.dump());
                return;
            }
        });
    });

    server_.POST("/api/v1/auth/login", [](const HttpReq* req, HttpResp* resp, SeriesWork* series) {
        // 解析请求 (抓包)
        // 处理业务逻辑
        if (req->content_type() != wfrest::APPLICATION_JSON) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "请求格式有误";
            resp->Json(ret.dump());
            return;
        }
        json js = json::parse(req->body());
        string username = js["username"];
        string password = js["password"];
        if (username.empty() || password.empty()) {
            resp->set_status(HttpStatusBadRequest);
            json ret = json::object();
            ret["status"] = "error";
            ret["message"] = "用户名和密码不能为空";
            resp->Json(ret.dump());
            return;
        }
        string sql = "SELECT * from tbl_user WHERE username='" + username + "';";
        cout << "[sql] : " << sql << endl;
        resp->MySQL(DatabaseURL, sql, [resp, password](MySQLResultCursor* cursor) {
            if (cursor->get_cursor_status() != MYSQL_STATUS_GET_RESULT) {
                resp->set_status(500);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "内部服务器错误";
                resp->Json(ret.dump());
                return;
            }
            if (cursor->get_rows_count() == 0) {
                resp->set_status(401);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "用户名或密码错误";
                resp->Json(ret.dump());
                return;
            }
            User user;
            map<string, MySQLCell> record;
            cursor->fetch_row(record);
            user.id = record["id"].as_int();
            user.pwhash = record["pwhash"].as_string();
            user.username = record["username"].as_string();
            user.createdAt = record["created_at"].as_string();
            if (CryptoUtil::verify_password(password, user.pwhash)) {
                string token = CryptoUtil::generate_token(user);
                resp->set_status(200);
                resp->add_header_pair("application", "json");
                json ret = json::object();
                ret["status"] = "success";
                ret["message"] = "登录成功";
                ret["data"]["accessToken"] = token;
                ret["data"]["tokenType"] = "Bearer";
                ret["data"]["user"]["userId"] = user.id;
                ret["data"]["username"]["username"] = user.username;
                resp->Json(ret.dump());
                return;
            } else {
                resp->set_status(401);
                json ret = json::object();
                ret["status"] = "error";
                ret["message"] = "用户名或密码错误";
                resp->Json(ret.dump());
                return;
            }
        });
        // 生成响应
    });
}
