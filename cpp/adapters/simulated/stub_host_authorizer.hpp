#pragma once
#include "emv/ports/host_authorizer.hpp"

namespace emv::simulated {

// Always approves. Demonstrates orchestration only; not a real issuer response or ARPC.
class StubHostAuthorizer final : public IHostAuthorizer {
public:
    HostResponse authorize(const HostRequest& request) override;
};

} // namespace emv::simulated
