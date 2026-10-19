#pragma once
#include "emv/domain.hpp"

namespace emv {

// Host boundary: HTTP client, ISO 8583 packager, mTLS and retry/idempotency policy belong here in production.
class IHostAuthorizer {
public:
    virtual ~IHostAuthorizer() = default;
    virtual HostResponse authorize(const HostRequest& request) = 0;
};

} // namespace emv
