#pragma once
#include "emv/domain.hpp"
#include <stdexcept>

namespace emv {

// Thrown when the host could not be reached or did not return a usable answer.
// The authorisation outcome is unknown: never treat it as an approval.
class HostUnavailableError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Host boundary: HTTP client, ISO 8583 packager, mTLS and retry/idempotency policy belong here in production.
class IHostAuthorizer {
public:
    virtual ~IHostAuthorizer() = default;
    // Returns the issuer's decision (approved or declined). Throws HostUnavailableError if there is none.
    virtual HostResponse authorize(const HostRequest& request) = 0;
};

} // namespace emv
