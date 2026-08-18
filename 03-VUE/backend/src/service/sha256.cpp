// SHA-256 implementation based on Brad Conte's public domain crypto-algorithms.
// https://github.com/B-Con/crypto-algorithms (public domain / CC0)

#include "sha256.h"

#include <cstring>

namespace crypto {

namespace {

constexpr uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

inline uint32_t rotright(uint32_t a, uint32_t b) { return (a >> b) | (a << (32 - b)); }
inline uint32_t ep0(uint32_t x) { return rotright(x, 2) ^ rotright(x, 13) ^ rotright(x, 22); }
inline uint32_t ep1(uint32_t x) { return rotright(x, 6) ^ rotright(x, 11) ^ rotright(x, 25); }
inline uint32_t sig0(uint32_t x) { return rotright(x, 7) ^ rotright(x, 18) ^ (x >> 3); }
inline uint32_t sig1(uint32_t x) { return rotright(x, 17) ^ rotright(x, 19) ^ (x >> 10); }

inline uint32_t load_be(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline void store_be(uint32_t v, uint8_t* p) {
    p[0] = uint8_t((v >> 24) & 0xff);
    p[1] = uint8_t((v >> 16) & 0xff);
    p[2] = uint8_t((v >> 8) & 0xff);
    p[3] = uint8_t(v & 0xff);
}

}  // namespace

Sha256::Sha256() : datalen_(0), bitlen_(0) {
    std::memcpy(state_, H0, sizeof(state_));
    std::memset(data_, 0, sizeof(data_));
}

void Sha256::transform() {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = load_be(data_ + i * 4);
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = sig1(w[i - 2]) + w[i - 7] + sig0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + ep1(e) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t t2 = ep0(a) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        data_[datalen_++] = data[i];
        if (datalen_ == SHA256_BLOCK_SIZE) {
            transform();
            bitlen_ += 512;
            datalen_ = 0;
        }
    }
}

void Sha256::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> Sha256::finalize() {
    uint32_t i = datalen_;

    if (datalen_ < 56) {
        data_[i++] = 0x80;
        while (i < 56) data_[i++] = 0x00;
    } else {
        data_[i++] = 0x80;
        while (i < 64) data_[i++] = 0x00;
        transform();
        std::memset(data_, 0, 56);
        i = 56;
    }

    bitlen_ += uint64_t(datalen_) * 8;
    data_[63] = uint8_t(bitlen_ & 0xff);
    data_[62] = uint8_t((bitlen_ >> 8) & 0xff);
    data_[61] = uint8_t((bitlen_ >> 16) & 0xff);
    data_[60] = uint8_t((bitlen_ >> 24) & 0xff);
    data_[59] = uint8_t((bitlen_ >> 32) & 0xff);
    data_[58] = uint8_t((bitlen_ >> 40) & 0xff);
    data_[57] = uint8_t((bitlen_ >> 48) & 0xff);
    data_[56] = uint8_t((bitlen_ >> 56) & 0xff);
    transform();

    std::vector<uint8_t> hash(SHA256_DIGEST_SIZE);
    for (int j = 0; j < 8; ++j) {
        store_be(state_[j], hash.data() + j * 4);
    }
    return hash;
}

std::vector<uint8_t> Sha256::hash(const uint8_t* data, size_t len) {
    Sha256 ctx;
    ctx.update(data, len);
    return ctx.finalize();
}

std::vector<uint8_t> Sha256::hash(const std::string& data) {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> hmacSha256(const uint8_t* key, size_t keylen,
                                const uint8_t* msg, size_t msglen) {
    uint8_t k0[SHA256_BLOCK_SIZE] = {0};
    if (keylen > SHA256_BLOCK_SIZE) {
        auto hashed = Sha256::hash(key, keylen);
        std::memcpy(k0, hashed.data(), hashed.size());
    } else {
        std::memcpy(k0, key, keylen);
    }

    uint8_t ipad[SHA256_BLOCK_SIZE];
    uint8_t opad[SHA256_BLOCK_SIZE];
    for (size_t i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        ipad[i] = k0[i] ^ 0x36;
        opad[i] = k0[i] ^ 0x5c;
    }

    Sha256 inner;
    inner.update(ipad, SHA256_BLOCK_SIZE);
    inner.update(msg, msglen);
    auto inner_hash = inner.finalize();

    Sha256 outer;
    outer.update(opad, SHA256_BLOCK_SIZE);
    outer.update(inner_hash.data(), inner_hash.size());
    return outer.finalize();
}

std::vector<uint8_t> hmacSha256(const std::string& key, const std::string& message) {
    return hmacSha256(reinterpret_cast<const uint8_t*>(key.data()), key.size(),
                     reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

}  // namespace crypto
