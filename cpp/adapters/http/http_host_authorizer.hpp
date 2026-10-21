#pragma once
#include "emv/ports/host_authorizer.hpp"
#include <chrono>
#include <string>

namespace emv::http {

// Sends the authorisation request as JSON to the acquirer host (the Spring Boot mock API).
// Production needs TLS/mTLS, authentication and idempotent retry/reversal handling here.
class HttpHostAuthorizer final : public IHostAuthorizer {
public:
    struct Settings {
        std::string baseUrl;  // scheme://host[:port], e.g. http://localhost:8080
        std::string authorizePath = "/api/authorize";
        std::chrono::seconds connectTimeout{3};
        std::chrono::seconds readTimeout{10};
    };

    explicit HttpHostAuthorizer(Settings settings);
    HostResponse authorize(const HostRequest& request) override;

private:
    Settings settings_;
};

} // namespace emv::http
