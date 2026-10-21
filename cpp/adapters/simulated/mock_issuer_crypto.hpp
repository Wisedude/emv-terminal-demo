#pragma once
#include "emv/tlv.hpp"
#include <cstdint>
#include <string>

// Stand-ins for issuer cryptography. A real ARQC/TC/AAC is a MAC computed by the card with a key derived
// from the issuer master key, and a real ARPC is computed by the issuer's HSM. These are deterministic
// placeholders with the right shapes so the card and issuer sides can check each other in a demo.
// NEVER use them for anything real. The Spring Boot mock issuer implements the same formulas.
namespace emv::simulated {

// 8-byte application cryptogram over the CDOL1-style inputs in the transaction data.
Bytes mockApplicationCryptogram(const TlvStore& transactionData, std::uint8_t cryptogramInformationData);

// ARPC method 1 shape without the key: ARQC XOR (ARC || 00 00 00 00 00 00).
Bytes mockArpc(const Bytes& arqc, const Bytes& authorisationResponseCode);

// Tag 91 value: ARPC (8 bytes) || ARC (2 bytes).
Bytes mockIssuerAuthenticationData(const Bytes& arqc, const std::string& responseCode);

// "00" -> 30 30 (the ARC is the response code in ASCII).
Bytes responseCodeToArc(const std::string& responseCode);

// A tag 72 template with one PUT DATA command (new lower consecutive offline limit, mock MAC).
Bytes sampleIssuerScript();

} // namespace emv::simulated
