#include "emv/config/emv_configuration.hpp"
#include "emv/crypto/sha1.hpp"
#include "emv/emv_tags.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace emv::config {
namespace {

constexpr Tag kTerminalMandatoryTags[] = {tags::TerminalType, tags::TransactionCurrencyCode,
                                          tags::TerminalCountryCode, tags::TerminalCapabilities};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw ConfigError("Cannot open EMV configuration file " + path.string());
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

const Bytes& requireTag(const TlvStore& record, Tag tag, const char* what) {
    const Bytes* value = record.find(tag);
    if (!value || value->empty()) throw ConfigError(std::string("missing ") + what + " (" + tagToHex(tag) + ")");
    return *value;
}

std::string recordLabel(const char* file, const ConfigRecord& record) {
    return std::string(file) + " record at line " + std::to_string(record.firstLine);
}

std::vector<AidConfig> loadAids(std::string_view text, const char* file, std::vector<std::string>& warnings) {
    std::vector<AidConfig> aids;
    for (const auto& record : parseConfigRecords(text, file)) {
        if (record.error) {
            warnings.push_back("Rejected " + recordLabel(file, record) + ": " + *record.error);
            continue;
        }
        try {
            AidConfig aid = toAidConfig(record.tags);
            const bool duplicate = std::any_of(aids.begin(), aids.end(),
                                               [&](const AidConfig& existing) { return existing.aid == aid.aid; });
            if (duplicate) {
                warnings.push_back("Ignored duplicate AID " + toHex(aid.aid) + " in " + recordLabel(file, record));
                continue;
            }
            aids.push_back(std::move(aid));
        } catch (const ConfigError& error) {
            warnings.push_back("Rejected " + recordLabel(file, record) + ": " + error.what());
        }
    }
    return aids;
}

std::vector<CaPublicKey> loadCaKeys(std::string_view text, std::vector<std::string>& warnings) {
    std::vector<CaPublicKey> keys;
    for (const auto& record : parseConfigRecords(text, kCaPublicKeysFile)) {
        if (record.error) {
            warnings.push_back("Rejected CA public key in " + recordLabel(kCaPublicKeysFile, record) + ": "
                               + *record.error);
            continue;
        }
        try {
            CaPublicKey key = toCaPublicKey(record.tags);
            const std::string name = toHex(key.rid) + "/" + toHex({key.index});
            if (!hasValidChecksum(key)) {
                warnings.push_back("Rejected CA public key " + name + ": SHA-1 checksum does not match");
                continue;
            }
            keys.push_back(std::move(key));
        } catch (const ConfigError& error) {
            warnings.push_back("Rejected CA public key in " + recordLabel(kCaPublicKeysFile, record) + ": "
                               + error.what());
        }
    }
    return keys;
}

TlvStore loadTerminal(std::string_view text) {
    const auto records = parseConfigRecords(text, kTerminalFile);
    if (records.size() != 1) throw ConfigError(std::string(kTerminalFile) + " must contain exactly one record");
    if (records.front().error) throw ConfigError(*records.front().error);
    for (const Tag tag : kTerminalMandatoryTags) {
        if (!records.front().tags.contains(tag)) {
            throw ConfigError(std::string(kTerminalFile) + " is missing mandatory tag " + tagToHex(tag) + " ("
                              + tagName(tag) + ")");
        }
    }
    return records.front().tags;
}

bool startsWith(const Bytes& value, const Bytes& prefix) {
    return prefix.size() <= value.size() && std::equal(prefix.begin(), prefix.end(), value.begin());
}

} // namespace

AidConfig toAidConfig(const TlvStore& record) {
    Bytes aid = requireTag(record, tags::AidTerminal, "AID");
    // The files may carry the AID's length as a leading byte ("9F06= 07 A0 00 00 00 03 10 10;").
    // An AID never starts with a byte equal to its own remaining length, so this is unambiguous.
    if (aid.size() >= 2 && static_cast<std::size_t>(aid.front()) == aid.size() - 1) aid.erase(aid.begin());
    if (aid.size() < 5 || aid.size() > 16) throw ConfigError("AID " + toHex(aid) + " must be 5 to 16 bytes");

    AidConfig config{aid, record};
    config.tags.set(tags::AidTerminal, aid);
    return config;
}

CaPublicKey toCaPublicKey(const TlvStore& record) {
    CaPublicKey key;
    key.rid = requireTag(record, tags::AidTerminal, "RID");
    const Bytes& index = requireTag(record, tags::CaPublicKeyIndex, "key index");
    key.modulus = requireTag(record, tags::CaPublicKeyModulus, "modulus");
    key.exponent = requireTag(record, tags::CaPublicKeyExponent, "exponent");
    key.checksum = requireTag(record, tags::CaPublicKeyChecksum, "checksum");
    if (key.rid.size() != 5) throw ConfigError("RID " + toHex(key.rid) + " must be 5 bytes");
    if (index.size() != 1) throw ConfigError("CA public key index must be 1 byte");
    if (key.checksum.size() != 20) throw ConfigError("CA public key checksum must be 20 bytes (SHA-1)");
    key.index = index.front();
    return key;
}

bool hasValidChecksum(const CaPublicKey& key) {
    Bytes data = key.rid;
    data.push_back(key.index);
    data.insert(data.end(), key.modulus.begin(), key.modulus.end());
    data.insert(data.end(), key.exponent.begin(), key.exponent.end());
    return crypto::sha1(data) == key.checksum;
}

const AidConfig* findSupportedAid(const std::vector<AidConfig>& supported, const Bytes& cardAid) {
    const AidConfig* best = nullptr;
    for (const auto& candidate : supported) {
        if (candidate.aid == cardAid) return &candidate;
        if (startsWith(cardAid, candidate.aid) && (!best || candidate.aid.size() > best->aid.size())) best = &candidate;
    }
    return best;
}

const CaPublicKey* findCaPublicKey(const EmvConfiguration& configuration, const Bytes& rid, std::uint8_t index) {
    for (const auto& key : configuration.caPublicKeys) {
        if (key.rid == rid && key.index == index) return &key;
    }
    return nullptr;
}

LoadResult buildEmvConfiguration(std::string_view terminalText, std::string_view contactAidsText,
                                 std::string_view contactlessAidsText, std::string_view caKeysText) {
    LoadResult result;
    result.configuration.terminal = loadTerminal(terminalText);
    result.configuration.contactAids = loadAids(contactAidsText, kContactAidsFile, result.warnings);
    result.configuration.contactlessAids = loadAids(contactlessAidsText, kContactlessAidsFile, result.warnings);
    result.configuration.caPublicKeys = loadCaKeys(caKeysText, result.warnings);
    return result;
}

LoadResult loadEmvConfiguration(const std::filesystem::path& directory) {
    return buildEmvConfiguration(readFile(directory / kTerminalFile), readFile(directory / kContactAidsFile),
                                 readFile(directory / kContactlessAidsFile), readFile(directory / kCaPublicKeysFile));
}

} // namespace emv::config
