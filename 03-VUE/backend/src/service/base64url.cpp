#include "base64url.h"

namespace crypto {

namespace {

constexpr char kEncodeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int8_t decodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return int8_t(c - 'A');
    if (c >= 'a' && c <= 'z') return int8_t(c - 'a' + 26);
    if (c >= '0' && c <= '9') return int8_t(c - '0' + 52);
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

}  // namespace

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | uint32_t(data[i + 2]);
        out.push_back(kEncodeTable[(n >> 18) & 0x3f]);
        out.push_back(kEncodeTable[(n >> 12) & 0x3f]);
        out.push_back(kEncodeTable[(n >> 6) & 0x3f]);
        out.push_back(kEncodeTable[n & 0x3f]);
        i += 3;
    }
    size_t rem = len - i;
    if (rem == 1) {
        uint32_t n = uint32_t(data[i]) << 16;
        out.push_back(kEncodeTable[(n >> 18) & 0x3f]);
        out.push_back(kEncodeTable[(n >> 12) & 0x3f]);
    } else if (rem == 2) {
        uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        out.push_back(kEncodeTable[(n >> 18) & 0x3f]);
        out.push_back(kEncodeTable[(n >> 12) & 0x3f]);
        out.push_back(kEncodeTable[(n >> 6) & 0x3f]);
    }
    return out;
}

std::string base64UrlEncode(const std::string& data) {
    return base64UrlEncode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string base64UrlEncode(const std::vector<uint8_t>& data) {
    return base64UrlEncode(data.data(), data.size());
}

std::vector<uint8_t> base64UrlDecode(const std::string& encoded) {
    std::vector<uint8_t> out;
    out.reserve(encoded.size() / 4 * 3);

    size_t i = 0;
    while (i < encoded.size()) {
        int8_t a = decodeChar(encoded[i]);
        int8_t b = (i + 1 < encoded.size()) ? decodeChar(encoded[i + 1]) : -1;
        int8_t c = (i + 2 < encoded.size()) ? decodeChar(encoded[i + 2]) : -1;
        int8_t d = (i + 3 < encoded.size()) ? decodeChar(encoded[i + 3]) : -1;
        if (a < 0 || b < 0) return {};

        uint32_t n = (uint32_t(a) << 18) | (uint32_t(b) << 12);
        out.push_back(uint8_t((n >> 16) & 0xff));
        if (c >= 0) {
            n |= uint32_t(c) << 6;
            out.push_back(uint8_t((n >> 8) & 0xff));
            if (d >= 0) {
                n |= uint32_t(d);
                out.push_back(uint8_t(n & 0xff));
            }
        }
        i += 4;
    }
    return out;
}

}  // namespace crypto
