#include "emv/config/emv_configuration.hpp"
#include "emv/crypto/sha1.hpp"
#include "emv/emv_tags.hpp"
#include "test_support.hpp"

namespace {

using namespace emv;
using namespace emv::config;
using namespace emv::test;

Bytes bytes(const char* hex) {
    return hexToBytes(hex);
}

void convertsHexAndTags() {
    require((hexToBytes("9F 26 0a") == Bytes{0x9F, 0x26, 0x0A}), "hex with spaces and lower case must parse");
    require(toHex(Bytes{0x9F, 0x26, 0x0A}) == "9F260A", "bytes must print as upper-case hex");
    requireThrows<std::invalid_argument>([] { hexToBytes("9F2"); }, "odd digit count must be rejected");
    requireThrows<std::invalid_argument>([] { hexToBytes("ZZ"); }, "non-hex characters must be rejected");
    require(tagToHex(0x95) == "95" && tagToHex(0x9F918704) == "9F918704", "tags print with their own length");
    require(tagFromHex("9F26") == 0x9F26, "tags parse from hex");
}

void encodesAndParsesBerTlv() {
    const TlvList fields{{0x9F26, Bytes(8, 0x11)}, {0x95, Bytes(5, 0x00)}, {0x72, Bytes(200, 0x86)}};
    const Bytes encoded = encodeTlv(fields);
    require(encoded[0] == 0x9F && encoded[1] == 0x26 && encoded[2] == 0x08, "short form length");
    const TlvList parsed = parseTlv(encoded);
    require(parsed.size() == 3 && parsed[0].tag == 0x9F26 && parsed[2].value.size() == 200,
            "round trip including the 81 long-form length");

    require(parseTlv(bytes("00 95 01 80 FF")).size() == 1, "00/FF padding between objects is skipped");
    requireThrows<std::invalid_argument>([] { parseTlv(bytes("9F 26 08 01")); }, "truncated value must be rejected");
}

void tlvStoreReplacesInPlace() {
    TlvStore store;
    store.set(0x9F02, {0x01});
    store.set(0x95, {0x00});
    store.set(0x9F02, {0x02});
    require(store.fields().size() == 2 && store.fields()[0].tag == 0x9F02, "set keeps the original position");
    require((store.at(0x9F02) == Bytes{0x02}), "set replaces the value");
    requireThrows<std::out_of_range>([&] { store.at(0x9F26); }, "missing tag must throw");
}

void sha1MatchesKnownVectors() {
    require(toHex(crypto::sha1(Bytes{'a', 'b', 'c'})) == "A9993E364706816ABA3E25717850C26C9CD0D89D", "SHA-1('abc')");
    require(toHex(crypto::sha1({})) == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709", "SHA-1('')");
}

void parsesConfigRecords() {
    const char* text =
        "# header comment\n"
        "9F06= 07 A0 00 00 00 03 10 10;   # Visa\n"
        "9F1B= 00 00 00 00;\n"
        "\n"
        "# second record\n"
        "9F06=A0000000041010; 9F09=0002;\n"
        "\n"
        "9F06=XYZ;\n";
    const auto records = parseConfigRecords(text, "T.TXT");
    require(records.size() == 3, "blank lines separate records; comment lines do not");
    require(records[0].firstLine == 2 && (records[0].tags.at(0x9F1B) == Bytes(4, 0x00)), "first record parsed");
    require(records[1].tags.contains(0x9F09), "several statements may share a line");
    require(records[2].error && records[2].error->find("T.TXT:8") == 0, "errors name the file and line");

    const auto missingSemicolon = parseConfigRecords("9F1A=0710\n", "X.TXT");
    require(missingSemicolon[0].error && missingSemicolon[0].error->find("';'") != std::string::npos,
            "a statement must end with ';'");
}

void normalisesAidsAndMatchesPartially() {
    TlvStore withLength;
    withLength.set(tags::AidTerminal, bytes("07 A0 00 00 00 03 10 10"));
    const AidConfig visa = toAidConfig(withLength);
    require(visa.aid == bytes("A0000000031010") && visa.tags.at(tags::AidTerminal) == visa.aid,
            "the leading length byte is removed");

    TlvStore ridOnly;
    ridOnly.set(tags::AidTerminal, bytes("A000000004"));
    const std::vector<AidConfig> supported{visa, toAidConfig(ridOnly)};
    require(findSupportedAid(supported, bytes("A0000000031010")) == &supported[0], "exact match");
    require(findSupportedAid(supported, bytes("A0000000041010")) == &supported[1], "partial selection by prefix");
    require(findSupportedAid(supported, bytes("A0000000651010")) == nullptr, "unsupported AID is not matched");

    TlvStore tooShort;
    tooShort.set(tags::AidTerminal, bytes("A0 00"));
    requireThrows<ConfigError>([&] { toAidConfig(tooShort); }, "AIDs shorter than 5 bytes are rejected");
}

void loadsShippedConfiguration() {
    const LoadResult result = loadEmvConfiguration(EMV_CONFIG_DIR);
    const EmvConfiguration& config = result.configuration;
    require(config.contactAids.size() == 7, "7 contact AIDs");
    require(config.contactlessAids.size() == 8, "8 contactless AIDs converted from CLESSCUST.TXT");
    require((config.terminal.at(tags::TerminalCapabilities) == bytes("E0F0C8")), "terminal capabilities loaded");

    require(config.caPublicKeys.size() == 19, "19 of the 26 CA keys pass validation");
    require(result.warnings.size() == 7, "each rejected CA key is reported");
    for (const auto& warning : result.warnings) {
        require(warning.find("Rejected CA public key") == 0, "warning names the rejected key: " + warning);
    }
    require(findCaPublicKey(config, bytes("A000000003"), 0x92) != nullptr, "Visa key 92 is loaded");
    require(findCaPublicKey(config, bytes("A000000004"), 0xF5) == nullptr, "Mastercard key F5 (bad checksum) is not");

    CaPublicKey tampered = *findCaPublicKey(config, bytes("A000000003"), 0x92);
    tampered.modulus[10] = static_cast<std::uint8_t>(tampered.modulus[10] ^ 0x01);
    require(!hasValidChecksum(tampered), "a modified modulus fails the checksum");
}

void terminalRecordNeedsMandatoryTags() {
    requireThrows<ConfigError>([] { buildEmvConfiguration("9F35=22;\n", "", "", ""); },
                               "TERMINAL.TXT without currency, country and capabilities must be rejected");
}

void describesTvrTsiAndCvmResults() {
    require((describeTvr(bytes("8000008000")) == std::vector<std::string>{
                "Offline data authentication was not performed", "Transaction exceeds floor limit"}),
            "TVR bits are described in order");
    require((describeTsi(bytes("1400")) == std::vector<std::string>{
                "Issuer authentication was performed", "Script processing was performed"}),
            "TSI bits are described");
    require(describeCvmResults(bytes("1F0002")) == "No CVM required · always · successful", "CVM Results");
    require(tagName(0x9F27) == "Cryptogram Information Data", "tag names");
}

} // namespace

int main() {
    return runTests("EmvConfig", {
        {"convertsHexAndTags", convertsHexAndTags},
        {"encodesAndParsesBerTlv", encodesAndParsesBerTlv},
        {"tlvStoreReplacesInPlace", tlvStoreReplacesInPlace},
        {"sha1MatchesKnownVectors", sha1MatchesKnownVectors},
        {"parsesConfigRecords", parsesConfigRecords},
        {"normalisesAidsAndMatchesPartially", normalisesAidsAndMatchesPartially},
        {"loadsShippedConfiguration", loadsShippedConfiguration},
        {"terminalRecordNeedsMandatoryTags", terminalRecordNeedsMandatoryTags},
        {"describesTvrTsiAndCvmResults", describesTvrTsiAndCvmResults},
    });
}
