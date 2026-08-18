#pragma once

#include <optional>
#include <string>

// HS256 JWT 自实现工具。依赖 sha256 + base64url，不依赖 jwt-cpp / OpenSSL。
// Header 固定 {"alg":"HS256","typ":"JWT"}。Payload 用 nlohmann/json 序列化。
class JwtHelper {
public:
    // ttlSec 为 token 有效期秒数。
    static std::string generate(const std::string& secret,
                                const std::string& username,
                                const std::string& issuer,
                                int64_t ttlSec);

    // 校验签名、issuer、exp。任意一项失败返回 std::nullopt。
    static std::optional<std::string> verify(const std::string& secret,
                                             const std::string& token,
                                             const std::string& issuer);
};
