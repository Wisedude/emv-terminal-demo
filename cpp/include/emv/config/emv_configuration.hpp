#pragma once
#include "emv/config/config_file.hpp"
#include "emv/tlv.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The data every EMV kernel is loaded with: terminal data, supported AIDs per interface and CA public
// keys. Kernel-neutral: a vendor kernel adapter feeds the same configuration into its SDK.
namespace emv::config {

struct AidConfig {
    Bytes aid;       // 5-16 bytes, without the optional length prefix used in the files
    TlvStore tags;   // every tag of the record, 9F06 normalised to the bare AID
};

struct CaPublicKey {
    Bytes rid;  // 5 bytes
    std::uint8_t index{0};
    Bytes modulus;
    Bytes exponent;
    Bytes checksum;  // SHA-1 over RID || index || modulus || exponent
};

struct EmvConfiguration {
    TlvStore terminal;
    std::vector<AidConfig> contactAids;
    std::vector<AidConfig> contactlessAids;
    std::vector<CaPublicKey> caPublicKeys;  // only keys whose checksum verified
};

struct LoadResult {
    EmvConfiguration configuration;
    std::vector<std::string> warnings;  // records that were rejected, and why
};

inline constexpr const char* kTerminalFile = "TERMINAL.TXT";
inline constexpr const char* kContactAidsFile = "AIDS_CONTACT.TXT";
inline constexpr const char* kContactlessAidsFile = "AIDS_CONTACTLESS.TXT";
inline constexpr const char* kCaPublicKeysFile = "CAKEYS.TXT";

// Reads the four files from a directory. Throws ConfigError if a file is missing or the terminal record
// is unusable; an invalid AID or CA key record is skipped and reported in warnings.
LoadResult loadEmvConfiguration(const std::filesystem::path& directory);

// Same, from file contents already in memory.
LoadResult buildEmvConfiguration(std::string_view terminalText, std::string_view contactAidsText,
                                 std::string_view contactlessAidsText, std::string_view caKeysText);

// Record -> model conversions; throw ConfigError when a mandatory tag is missing or malformed.
AidConfig toAidConfig(const TlvStore& record);
CaPublicKey toCaPublicKey(const TlvStore& record);

bool hasValidChecksum(const CaPublicKey& key);

// EMV Book 1, 12.3: an exact AID match wins; otherwise the longest terminal AID that is a prefix of the
// card's AID (partial selection). Returns nullptr when the terminal does not support the application.
const AidConfig* findSupportedAid(const std::vector<AidConfig>& supported, const Bytes& cardAid);

const CaPublicKey* findCaPublicKey(const EmvConfiguration& configuration, const Bytes& rid, std::uint8_t index);

} // namespace emv::config
