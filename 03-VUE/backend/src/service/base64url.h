#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crypto {

std::string base64UrlEncode(const uint8_t* data, size_t len);
std::string base64UrlEncode(const std::string& data);
std::string base64UrlEncode(const std::vector<uint8_t>& data);

// 解码失败返回空 vector
std::vector<uint8_t> base64UrlDecode(const std::string& encoded);

}  // namespace crypto
