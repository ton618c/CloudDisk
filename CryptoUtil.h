#pragma once // 避免头文件重复包含

#include <jwt.h>
#include <sodium.h>
#include <stdexcept>
#include <string>

struct User {
    int id;
    std::string username;
    std::string pwhash;
    std::string createdAt;
    // ...
};

// 静态工具类
class CryptoUtil {
public:
    // 生成data的SHA-256哈希值
    static std::string generate_hashcode(const unsigned char* data, size_t n);
    static std::string hash_password(const std::string& password);
    static bool verify_password(const std::string& password, const std::string& pwhash);
    static std::string generate_token(const User& user, jwt_alg_t algorithm = JWT_ALG_HS256);
    static bool verify_token(const std::string& token, User& user);

private:
    // 工具类只包含静态方法和静态数据，不应该被实例化。
    // 禁用无参构造函数、拷贝构造函数、赋值操作、析构函数
    CryptoUtil() = delete;
    ~CryptoUtil() = delete;
    CryptoUtil(const CryptoUtil&) = delete;
    CryptoUtil& operator=(const CryptoUtil&) = delete;

public:
    // 确保初始化libsodium
    struct SodiumInitializer {
        // 在SodiumInitializer的构造函数中，初始化libsodium
        SodiumInitializer()
        {
            if (sodium_init() < 0) {
                throw std::runtime_error("libsodium初始化失败");
            }
        }
    };

private:
    static SodiumInitializer initializer;
};
