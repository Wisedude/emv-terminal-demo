#include "emv/crypto/sha1.hpp"
#include <array>
#include <cstdint>

namespace emv::crypto {
namespace {

std::uint32_t rotateLeft(std::uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

void processBlock(const std::uint8_t* block, std::array<std::uint32_t, 5>& state) {
    std::array<std::uint32_t, 80> w{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{block[i * 4]} << 24) | (std::uint32_t{block[i * 4 + 1]} << 16)
             | (std::uint32_t{block[i * 4 + 2]} << 8) | std::uint32_t{block[i * 4 + 3]};
    }
    for (int i = 16; i < 80; ++i) w[i] = rotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const std::uint32_t temp = rotateLeft(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

} // namespace

Bytes sha1(const Bytes& data) {
    std::array<std::uint32_t, 5> state{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    Bytes message = data;
    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    message.push_back(0x80);
    while (message.size() % 64 != 56) message.push_back(0x00);
    for (int shift = 56; shift >= 0; shift -= 8) message.push_back(static_cast<std::uint8_t>(bitLength >> shift));

    for (std::size_t offset = 0; offset < message.size(); offset += 64) processBlock(&message[offset], state);

    Bytes digest;
    for (const auto word : state) {
        for (int shift = 24; shift >= 0; shift -= 8) digest.push_back(static_cast<std::uint8_t>(word >> shift));
    }
    return digest;
}

} // namespace emv::crypto
