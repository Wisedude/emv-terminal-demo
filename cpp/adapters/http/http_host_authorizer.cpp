#include "http/http_host_authorizer.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <optional>
#include <utility>

namespace emv::http {
namespace {

using nlohmann::json;

json toWireRequest(const HostRequest& request) {
    return {
        {"amountMinor", request.amountMinor},
        {"currency", request.currencyCode},
        {"aid", request.aid},
        {"transactionId", request.transactionId},
        {"cryptogramType", toString(request.cryptogramType)},
        {"panToken", request.cardToken},
        {"iccData", toHex(request.iccData)},
    };
}

std::string optionalString(const json& body, const char* field) {
    const auto value = body.find(field);
    return value == body.end() || value->is_null() ? std::string{} : value->get<std::string>();
}

std::optional<Bytes> optionalHex(const json& body, const char* field) {
    const std::string text = optionalString(body, field);
    if (text.empty()) return std::nullopt;
    try {
        return hexToBytes(text);
    } catch (const std::invalid_argument&) {
        throw HostUnavailableError(std::string("Host response field ") + field + " is not valid hex");
    }
}

HostResponse fromWireResponse(const json& body, const std::string& expectedTransactionId) {
    if (body.at("transactionId").get<std::string>() != expectedTransactionId) {
        throw HostUnavailableError("Host response is for a different transaction");
    }
    HostResponse response;
    response.approved = body.at("approved").get<bool>();
    response.responseCode = body.at("responseCode").get<std::string>();
    response.authorizationCode = optionalString(body, "authorizationCode");
    response.message = body.at("message").get<std::string>();
    response.issuerAuthenticationData = optionalHex(body, "issuerAuthenticationData");
    response.issuerScripts = optionalHex(body, "issuerScripts");
    if (response.responseCode.empty()) throw HostUnavailableError("Host response has no response code");
    return response;
}

// Spring Boot error bodies carry the reason in "message"; fall back to the raw body.
std::string errorReason(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_object() && parsed.contains("message") && parsed["message"].is_string()) {
        return parsed["message"].get<std::string>();
    }
    return body;
}

} // namespace

HttpHostAuthorizer::HttpHostAuthorizer(Settings settings) : settings_(std::move(settings)) {
    if (settings_.baseUrl.empty()) throw std::invalid_argument("Host base URL is required");
}

HostResponse HttpHostAuthorizer::authorize(const HostRequest& request) {
    httplib::Client client(settings_.baseUrl);
    client.set_connection_timeout(settings_.connectTimeout);
    client.set_read_timeout(settings_.readTimeout);

    std::cout << "[Host] POST " << settings_.baseUrl << settings_.authorizePath
              << " transaction=" << request.transactionId << ", cryptogram=" << toString(request.cryptogramType)
              << ", DE 55 " << request.iccData.size() << " bytes\n";
    const auto reply = client.Post(settings_.authorizePath, toWireRequest(request).dump(), "application/json");
    if (!reply) {
        throw HostUnavailableError("Authorization host unreachable at " + settings_.baseUrl + ": "
                                   + httplib::to_string(reply.error()));
    }
    if (reply->status != 200) {
        throw HostUnavailableError("Authorization host returned HTTP " + std::to_string(reply->status) + ": "
                                   + errorReason(reply->body));
    }

    try {
        return fromWireResponse(json::parse(reply->body), request.transactionId);
    } catch (const json::exception& error) {
        throw HostUnavailableError(std::string("Authorization host returned an invalid response: ") + error.what());
    }
}

} // namespace emv::http
