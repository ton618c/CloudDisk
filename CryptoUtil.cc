#include "CryptoUtil.h"
#include <jwt.h>
#include <sodium/crypto_hash_sha256.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

using namespace std;

// 链接属性
// 内部链接: static
// 外部链接：默认
static const char* SECRET_KEY = "$^Hk16NV";

CryptoUtil::SodiumInitializer initializer;

std::string CryptoUtil::generate_hashcode(const unsigned char* data, size_t n)
{
    unsigned char output[crypto_hash_sha256_BYTES];
    if (crypto_hash_sha256(output, data, n) != 0) {
        throw std::runtime_error("生成SHA256哈希失败");
    }
    // 转换成十六进制字符，存储到result中
    char result[crypto_hash_sha256_BYTES * 2 + 1] = "";
    for (unsigned i = 0; i < crypto_hash_sha256_BYTES; ++i) {
        sprintf(result + 2 * i, "%02x", output[i]);
    }
    return result;
}

string CryptoUtil::hash_password(const string& password)
{
    // 输出缓冲区，用于存储生成的哈希字符串，必须为`crypto_pwhash_STRBYTES`字节 (128字节)
    char hashed[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hashed,
            password.c_str(),
            password.length(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE, // 操作限制(operation limit)
            crypto_pwhash_MEMLIMIT_INTERACTIVE) // 内存限制(memory limit)
        != 0) {
        throw std::runtime_error("生成密码的哈希值失败");
    }
    return string(hashed);
}

bool CryptoUtil::verify_password(const string& password, const string& pwhash)
{
    return crypto_pwhash_str_verify(pwhash.c_str(),
               password.c_str(),
               password.length())
        == 0;
}

string CryptoUtil::generate_token(const User& user, jwt_alg_t algorithm)
{
    jwt_t* jwt;
    jwt_new(&jwt); // 创建 JWT：申请空间

    jwt_set_alg(jwt, algorithm, (unsigned char*)SECRET_KEY, strlen(SECRET_KEY));

    // 设置载荷(Payload): 用户自定义数据(不能存放敏感数据，比如：密码的哈希值)
    jwt_add_grant(jwt, "sub", "login");
    jwt_add_grant_int(jwt, "id", user.id); // 用户id
    jwt_add_grant(jwt, "username", user.username.c_str()); // 用户名字
    jwt_add_grant_int(jwt, "expire", time(NULL) + 1800); // 过期时间 (30min)

    char* token = jwt_encode_str(jwt); // token长度是不确定的，100-300字节
    string result = token;
    // 释放资源
    jwt_free(jwt);
    free(token);

    return result;
}

bool CryptoUtil::verify_token(const std::string& token, User& user)
{
    jwt_t* jwt;
    int err = jwt_decode(&jwt, token.c_str(), (unsigned char*)SECRET_KEY, strlen(SECRET_KEY));
    if (err) {
        return false;
    }

    // 验证主题
    if (strcmp("login", jwt_get_grant(jwt, "sub")) != 0) {
        jwt_free(jwt);
        return false;
    }
    // 判断是否过期
    if (jwt_get_grant_int(jwt, "expire") < time(NULL)) {
        jwt_free(jwt);
        return false;
    }
    // 解析 token
    user.id = jwt_get_grant_int(jwt, "id");
    user.username = jwt_get_grant(jwt, "username");

    jwt_free(jwt);
    return true;
}
