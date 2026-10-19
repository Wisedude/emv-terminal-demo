#include "simulated/stub_host_authorizer.hpp"
#include <iostream>

namespace emv::simulated {

HostResponse StubHostAuthorizer::authorize(const HostRequest& request) {
    std::cout << "[Host] JSON/ISO host boundary; transaction=" << request.transactionId
              << ", cryptogram=" << toString(request.cryptogramType) << "\n";
    return {true, "00", "D3M042", std::string("DEMO-ARPC-NOT-VALID"), "Mock issuer approval"};
}

} // namespace emv::simulated
