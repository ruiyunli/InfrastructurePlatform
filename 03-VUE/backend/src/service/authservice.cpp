#include "authservice.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

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
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24);

    return jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("login-backend")
        .set_type("JWT")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("username", json(username))
        .sign(jwt::algorithm::hs256{jwtSecret_});
}

std::string AuthService::verifyToken(const std::string& token) const {
    auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
        .allow_algorithm(jwt::algorithm::hs256{jwtSecret_})
        .with_issuer("login-backend");

    auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);
    verifier.verify(decoded);

    return decoded.get_payload_claim("username").as_string();
}

bool AuthService::verifyUser(const std::string& username, const std::string& password) {
    for (const auto& user : users_) {
        if (user.username == username && user.password == password) {
            return true;
        }
    }
    return false;
}
