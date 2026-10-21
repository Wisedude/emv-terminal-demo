#include "simulated/mock_issuer_crypto.hpp"
#include "emv/crypto/sha1.hpp"
#include "emv/emv_tags.hpp"

namespace emv::simulated {

Bytes mockApplicationCryptogram(const TlvStore& transactionData, std::uint8_t cryptogramInformationData) {
    static constexpr Tag kInputs[] = {tags::AmountAuthorised, tags::AmountOther, tags::TerminalCountryCode,
                                      tags::TerminalVerificationResults, tags::TransactionCurrencyCode,
                                      tags::TransactionDate, tags::TransactionType, tags::UnpredictableNumber,
                                      tags::ApplicationInterchangeProfile, tags::ApplicationTransactionCounter};
    Bytes input;
    for (const Tag tag : kInputs) {
        const Bytes value = transactionData.valueOr(tag, {});
        input.insert(input.end(), value.begin(), value.end());
    }
    input.push_back(cryptogramInformationData);
    const Bytes digest = crypto::sha1(input);
    return Bytes(digest.begin(), digest.begin() + 8);
}

Bytes responseCodeToArc(const std::string& responseCode) {
    Bytes arc(responseCode.begin(), responseCode.end());
    arc.resize(2, 0x20);
    return arc;
}

Bytes mockArpc(const Bytes& arqc, const Bytes& authorisationResponseCode) {
    Bytes arpc = arqc;
    arpc.resize(8, 0x00);
    for (std::size_t i = 0; i < authorisationResponseCode.size() && i < arpc.size(); ++i) {
        arpc[i] = static_cast<std::uint8_t>(arpc[i] ^ authorisationResponseCode[i]);
    }
    return arpc;
}

Bytes mockIssuerAuthenticationData(const Bytes& arqc, const std::string& responseCode) {
    const Bytes arc = responseCodeToArc(responseCode);
    Bytes data = mockArpc(arqc, arc);
    data.insert(data.end(), arc.begin(), arc.end());
    return data;
}

Bytes sampleIssuerScript() {
    // 72 13 | 9F18 04 00000001 | 86 0A 04 DA 9F 58 05 0A 11 22 33 44
    return hexToBytes("72 13 9F 18 04 00 00 00 01 86 0A 04 DA 9F 58 05 0A 11 22 33 44");
}

} // namespace emv::simulated
