#include "authservice.h"
#include "jwt_helper.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

AuthService::AuthService(const std::string& secret, const std::string& usersFile)
    : jwtSecret_(secret) {
    loadUsersFromFile(usersFile);
}

void AuthService::loadUsersFromFile(const std::string& usersFile) {
    std::ifstream in(usersFile);
    if (!in.is_open()) {
        users_ = {{"admin", "admin"}};
        return;
    }

    json data;
    in >> data;
    for (const auto& item : data) {
        users_.push_back({item["username"], item["password"]});
    }
}

std::string AuthService::generateToken(const std::string& username) {
    constexpr int64_t kTtlSec = 24 * 60 * 60;  // 24 小时
    return JwtHelper::generate(jwtSecret_, username, "login-backend", kTtlSec);
}

std::string AuthService::verifyToken(const std::string& token) const {
    auto username = JwtHelper::verify(jwtSecret_, token, "login-backend");
    if (!username) {
        throw std::runtime_error("token invalid or expired");
    }
    return *username;
}

bool AuthService::verifyUser(const std::string& username, const std::string& password) {
    for (const auto& user : users_) {
        if (user.username == username && user.password == password) {
            return true;
        }
    }
    return false;
}
