#pragma once
#include "emv/tlv.hpp"

namespace emv::crypto {

// SHA-1 (FIPS 180-4). EMV uses it for CA public key checksums (Book 2, 11.2); it is not used here for
// anything secret.
Bytes sha1(const Bytes& data);

} // namespace emv::crypto
