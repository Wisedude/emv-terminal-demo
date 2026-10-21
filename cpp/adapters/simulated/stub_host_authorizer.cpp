#include "simulated/stub_host_authorizer.hpp"
#include "emv/emv_tags.hpp"
#include "simulated/mock_issuer_crypto.hpp"
#include <iostream>

namespace emv::simulated {

HostResponse StubHostAuthorizer::authorize(const HostRequest& request) {
    Bytes arqc;
    for (const auto& field : parseTlv(request.iccData)) {
        if (field.tag == tags::ApplicationCryptogram) arqc = field.value;
    }
    if (arqc.empty()) throw HostUnavailableError("Stub host: ICC data has no application cryptogram (9F26)");

    std::cout << "[Host] Stub issuer: transaction=" << request.transactionId << ", cryptogram="
              << toString(request.cryptogramType) << ", DE 55 " << request.iccData.size() << " bytes\n";
    return {true, "00", "D3M042", mockIssuerAuthenticationData(arqc, "00"), "Mock issuer approval",
            sampleIssuerScript()};
}

} // namespace emv::simulated
