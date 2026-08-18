#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crypto {

constexpr size_t SHA256_DIGEST_SIZE = 32;
constexpr size_t SHA256_BLOCK_SIZE = 64;

class Sha256 {
public:
    Sha256();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& data);
    std::vector<uint8_t> finalize();

    static std::vector<uint8_t> hash(const uint8_t* data, size_t len);
    static std::vector<uint8_t> hash(const std::string& data);

private:
    void transform();

    uint8_t data_[SHA256_BLOCK_SIZE];
    uint32_t datalen_;
    unsigned long long bitlen_;
    uint32_t state_[8];
};

std::vector<uint8_t> hmacSha256(const uint8_t* key, size_t keylen,
                                const uint8_t* msg, size_t msglen);
std::vector<uint8_t> hmacSha256(const std::string& key, const std::string& message);

}  // namespace crypto
