#include "terminal_agent/agent_protocol.hpp"
#include "emv/emv_tags.hpp"
#include <stdexcept>

namespace emv::agent {
namespace {

using nlohmann::json;

constexpr std::size_t kMaxTransactionIdLength = 64;

const json& requireField(const json& body, const char* field) {
    const auto value = body.find(field);
    if (value == body.end() || value->is_null()) throw std::invalid_argument(std::string(field) + " is required");
    return *value;
}

std::string requireString(const json& body, const char* field) {
    const json& value = requireField(body, field);
    if (!value.is_string()) throw std::invalid_argument(std::string(field) + " must be a string");
    return value.get<std::string>();
}

std::optional<std::string> optionalString(const json& body, const char* field) {
    if (!body.contains(field) || body[field].is_null()) return std::nullopt;
    return requireString(body, field);
}

json cryptogramJson(CryptogramType type, const std::string& opaqueValue) {
    return {{"type", toString(type)}, {"value", opaqueValue}};
}

json optionalHex(const std::optional<Bytes>& value) {
    return value ? json(toHex(*value)) : json(nullptr);
}

json hostJson(const HostResponse& host) {
    return {
        {"approved", host.approved},
        {"responseCode", host.responseCode},
        {"authorizationCode", host.authorizationCode.empty() ? json(nullptr) : json(host.authorizationCode)},
        {"issuerAuthenticationData", optionalHex(host.issuerAuthenticationData)},
        {"issuerScripts", optionalHex(host.issuerScripts)},
        {"message", host.message},
    };
}

const Bytes* findTag(const TlvList& data, Tag tag) {
    for (const auto& field : data) {
        if (field.tag == tag) return &field.value;
    }
    return nullptr;
}

json flagsJson(const TlvList& data, Tag tag, std::vector<std::string> (*describe)(const Bytes&)) {
    const Bytes* value = findTag(data, tag);
    if (!value) return nullptr;
    return {{"hex", toHex(*value)}, {"flags", describe(*value)}};
}

json emvJson(const TlvList& data) {
    json tagList = json::array();
    for (const auto& field : data) {
        tagList.push_back({{"tag", tagToHex(field.tag)}, {"name", tagName(field.tag)}, {"value", toHex(field.value)}});
    }
    const Bytes* cvm = findTag(data, tags::CvmResults);
    return {
        {"tvr", flagsJson(data, tags::TerminalVerificationResults, describeTvr)},
        {"tsi", flagsJson(data, tags::TransactionStatusInformation, describeTsi)},
        {"cvmResults", cvm ? json{{"hex", toHex(*cvm)}, {"description", describeCvmResults(*cvm)}} : json(nullptr)},
        {"tags", tagList},
    };
}

} // namespace

SaleCommand parseSaleCommand(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object()) throw std::invalid_argument("Request body must be a JSON object");

    SaleCommand command;

    const json& amount = requireField(parsed, "amountMinor");
    if (!amount.is_number_integer() || amount.get<std::int64_t>() <= 0) {
        throw std::invalid_argument("amountMinor must be a positive integer");
    }
    command.amountMinor = amount.get<std::int64_t>();

    const auto mode = entryModeFromString(requireString(parsed, "entryMode"));
    if (!mode) throw std::invalid_argument("entryMode must be one of Tap, Insert, Swipe, Manual");
    command.entryMode = *mode;

    command.transactionId = requireString(parsed, "transactionId");
    if (command.transactionId.empty() || command.transactionId.size() > kMaxTransactionIdLength) {
        throw std::invalid_argument("transactionId must be 1-64 characters");
    }

    command.kernel = optionalString(parsed, "kernel");

    if (const auto card = optionalString(parsed, "simulatedCard")) {
        const auto id = simulated::simulatedCardFromString(*card);
        if (!id) {
            throw std::invalid_argument("simulatedCard must be one of VISA, MASTERCARD, VISA_EXPIRED, "
                                        "MASTERCARD_UNKNOWN_CA_KEY, JCB_UNSUPPORTED");
        }
        command.simulation.card = *id;
    }
    if (const auto behaviour = optionalString(parsed, "simulatedCardBehaviour")) {
        const auto value = simulated::cardBehaviourFromString(*behaviour);
        if (!value) throw std::invalid_argument("simulatedCardBehaviour must be one of NORMAL, FORCE_ONLINE, DECLINE");
        command.simulation.cardBehaviour = *value;
    }
    return command;
}

json stageEvent(TransactionStage stage, StageStatus status, const std::string& detail) {
    return {{"type", "stage"}, {"stage", toString(stage)}, {"status", toString(status)}, {"detail", detail}};
}

json resultEvent(const SaleCommand& command, const std::string& kernelName, const TransactionResult& result) {
    return {
        {"type", "result"},
        {"result", {
            {"transactionId", command.transactionId},
            {"kernel", kernelName},
            {"entryMode", toString(command.entryMode)},
            {"amountMinor", command.amountMinor},
            {"approved", result.approved},
            {"message", result.message},
            {"selectedAid", result.selectedAid},
            {"firstCryptogram", cryptogramJson(result.first.type, result.first.opaqueCryptogram)},
            {"finalCryptogram", result.final ? cryptogramJson(result.final->type, result.final->opaqueCryptogram)
                                             : json(nullptr)},
            {"host", result.host ? hostJson(*result.host) : json(nullptr)},
            {"issuerScriptsProcessed", result.issuerScriptsProcessed},
            {"issuerScriptStatus", result.issuerScriptStatus},
            {"card", simulated::toString(command.simulation.card)},
            {"onlineIccData", result.host ? json(toHex(result.first.iccData)) : json(nullptr)},
            {"emv", emvJson(result.emvData)},
        }},
    };
}

json errorEvent(const std::string& code, const std::string& message) {
    return {{"type", "error"}, {"code", code}, {"message", message}};
}

std::string toLine(const json& event) {
    return event.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
}

} // namespace emv::agent
