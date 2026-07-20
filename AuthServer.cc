#include <signal.h>
#include <workflow/MySQLResult.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/Workflow.h>
#include <workflow/mysql_types.h>

#include <filesystem>

#include "CryptoUtil.h"
#include "auth.srpc.h"
#include "workflow/WFFacilities.h"
using namespace srpc;
using namespace std;
using namespace protocol;
static WFFacilities::WaitGroup waitGroup(1);
static const string DatabaseURL = "mysql://root:123456@localhost/CloudDisk";
static const int RetryMax = 3;
void sighandler(int) { waitGroup.done(); }

class AuthService : public Auth::Service {
public:
    void AuthRegister(
        RegisterRequest *request, RegisterResponse *response, srpc::RPCContext *ctx) override {
        string username = request->username();
        string password = request->password();
        string confirm = request->confirm();
        if (username.empty() || password.empty()) {
            response->set_code(400);
            response->set_status("error");
            response->set_message("用户名和密码不能为空");
            return;
        }
        if (!(password == confirm)) {
            response->set_code(400);
            response->set_status("error");
            response->set_message("两次输入的密码不一致");
            return;
        }
        // 生成响应
        string pwhash = CryptoUtil::hash_password(password);
        string sql = "INSERT into tbl_user (username , pwhash) values ('" + username + "' , '" +
                     pwhash + "');";
        cout << "[sql] : " << sql << endl;
        WFMySQLTask *mysql_task = WFTaskFactory::create_mysql_task(
            DatabaseURL, RetryMax, [username, response](WFMySQLTask *task) {
                MySQLResultCursor cursor(task->get_resp());
                if (cursor.get_cursor_status() == MYSQL_STATUS_OK &&
                    cursor.get_affected_rows() == 1) {
                    response->set_code(201);
                    response->set_status("success");
                    response->set_message("注册成功");
                    int id = cursor.get_insert_id();
                    response->set_userid(id);
                    response->set_username(username);
                    filesystem::create_directories("upload_files/" + username);
                } else {
                    response->set_code(409);
                    response->set_status("error");
                    response->set_message("用户名已存在");
                }
            });
        mysql_task->get_req()->set_query(sql);
        SeriesWork *series = ctx->get_series();
        series->push_back(mysql_task);
    }

    void AuthLogin(LoginRequest *request, LoginResponse *response, srpc::RPCContext *ctx) override {
        string username = request->username();
        string password = request->password();
        if (username.empty() || password.empty()) {
            response->set_code(400);
            response->set_status("error");
            response->set_message("用户名和密码不能为空");
            return;
        }
        string sql = "SELECT * from tbl_user WHERE username='" + username + "';";
        cout << "[sql] : " << sql << endl;
        WFMySQLTask *mysql_task = WFTaskFactory::create_mysql_task(
            DatabaseURL, RetryMax, [response, password](WFMySQLTask *task) {
                MySQLResultCursor cursor(task->get_resp());
                if (task->get_resp()->get_packet_type() == MYSQL_PACKET_ERROR) {
                    response->set_code(500);
                    response->set_status("error");
                    response->set_message("内部服务器错误");
                    return;
                }
                if (cursor.get_rows_count() == 0) {
                    response->set_code(401);
                    response->set_status("error");
                    response->set_message("用户名或密码错误");
                    return;
                }
                User user;
                map<string, MySQLCell> record;
                cursor.fetch_row(record);
                user.id = record["id"].as_int();
                user.pwhash = record["pwhash"].as_string();
                user.username = record["username"].as_string();
                user.createdAt = record["created_at"].as_string();
                if (CryptoUtil::verify_password(password, user.pwhash)) {
                    string token = CryptoUtil::generate_token(user);
                    response->set_code(200);
                    response->set_status("success");
                    response->set_message("登录成功");
                    response->set_accesstoken(token);
                    response->set_tokentype("Bearer");
                    response->set_userid(user.id);
                    response->set_username(user.username);
                    return;
                } else {
                    response->set_code(401);
                    response->set_status("error");
                    response->set_message("用户名或密码错误");
                    return;
                }
            });
        mysql_task->get_req()->set_query(sql);
        SeriesWork *series = ctx->get_series();
        series->push_back(mysql_task);
    }
};

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    signal(SIGINT, sighandler);
    SRPCServer server;

    AuthService authService;
    server.add_service(&authService);

    if (server.start(1314) == 0) {
        waitGroup.wait();
        server.stop();
    } else {
        cerr << "Error: Server start FAILED!" << endl;
    }
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
