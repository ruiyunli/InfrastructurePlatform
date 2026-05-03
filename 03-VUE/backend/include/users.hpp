#pragma once
#include <string>
#include <vector>
#include <optional>

// 用户结构体
struct User {
    std::string username;
    std::string password;
};

// 硬编码的用户数据
static const std::vector<User> users = {
    {"admin", "admin"},
    {"111", "111"},
    {"222", "222"}
};

// 查找用户函数
std::optional<User> findUser(const std::string& username, const std::string& password) {
    for (const auto& user : users) {
        if (user.username == username && user.password == password) {
            return user;
        }
    }
    return std::nullopt;
}
