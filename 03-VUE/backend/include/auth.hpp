#pragma once
#include <string>
#include <chrono>
#include <json.hpp>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

// 硬编码的JWT密钥
const std::string JWT_SECRET = "my_secret_key_2026";

// 生成JWT token
std::string generateToken(const std::string& username) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24); // 24小时有效期

    return jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("login-backend")
        .set_type("JWT")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("username", nlohmann::json(username))
        .sign(jwt::algorithm::hs256{JWT_SECRET});
}

// 验证JWT token并返回用户名
std::string verifyToken(const std::string& token) {
    auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
        .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
        .with_issuer("login-backend");

    auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);
    verifier.verify(decoded);

    return decoded.get_payload_claim("username").as_string();
}
