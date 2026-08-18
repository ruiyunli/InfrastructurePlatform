#include "jwt_helper.h"
#include "sha256.h"
#include "base64url.h"
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

namespace {

std::string encodeHeader() {
    json h = {{"alg", "HS256"}, {"typ", "JWT"}};
    return crypto::base64UrlEncode(h.dump());
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= uint8_t(a[i]) ^ uint8_t(b[i]);
    return diff == 0;
}

int64_t nowUnixSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

std::string JwtHelper::generate(const std::string& secret,
                                const std::string& username,
                                const std::string& issuer,
                                int64_t ttlSec) {
    int64_t iat = nowUnixSec();
    int64_t exp = iat + ttlSec;

    json payload = {
        {"iss", issuer},
        {"iat", iat},
        {"exp", exp},
        {"username", username},
    };

    std::string headerB64 = encodeHeader();
    std::string payloadB64 = crypto::base64UrlEncode(payload.dump());
    std::string signingInput = headerB64 + "." + payloadB64;
    auto sig = crypto::hmacSha256(secret, signingInput);
    std::string sigB64 = crypto::base64UrlEncode(sig);
    return signingInput + "." + sigB64;
}

std::optional<std::string> JwtHelper::verify(const std::string& secret,
                                             const std::string& token,
                                             const std::string& issuer) {
    size_t first = token.find('.');
    size_t second = (first == std::string::npos) ? std::string::npos
                                                 : token.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos) return std::nullopt;
    if (token.find('.', second + 1) != std::string::npos) return std::nullopt;

    std::string headerB64 = token.substr(0, first);
    std::string payloadB64 = token.substr(first + 1, second - first - 1);
    std::string sigB64 = token.substr(second + 1);

    std::string signingInput = headerB64 + "." + payloadB64;
    auto expectedSig = crypto::hmacSha256(secret, signingInput);
    std::string expectedSigB64 = crypto::base64UrlEncode(expectedSig);

    if (!constantTimeEquals(expectedSigB64, sigB64)) return std::nullopt;

    auto payloadBytes = crypto::base64UrlDecode(payloadB64);
    if (payloadBytes.empty()) return std::nullopt;
    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());

    try {
        auto payload = json::parse(payloadStr);
        if (payload.value("iss", std::string()) != issuer) return std::nullopt;
        int64_t exp = payload.value("exp", int64_t(0));
        if (nowUnixSec() > exp) return std::nullopt;
        std::string username = payload.value("username", std::string());
        if (username.empty()) return std::nullopt;
        return username;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}
