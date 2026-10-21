#pragma once
// JSON wire format between the UI and the terminal agent.
//
// Request  POST /terminal/transactions
//   { "amountMinor": 12500, "entryMode": "Insert", "transactionId": "SIM-1",
//     "kernel": "simulated-contact",          (optional, default by entry mode)
//     "simulatedCard": "VISA",                (optional: VISA | MASTERCARD | VISA_EXPIRED |
//                                              MASTERCARD_UNKNOWN_CA_KEY | JCB_UNSUPPORTED)
//     "simulatedCardBehaviour": "NORMAL" }    (optional: NORMAL | FORCE_ONLINE | DECLINE)
//
// Response: application/x-ndjson, one event per line, in order:
//   { "type": "stage",  "stage": "READ_APPLICATION_DATA", "status": "STARTED|COMPLETED|SKIPPED", "detail": "..." }
//   ... then exactly one of
//   { "type": "result", "result": { ..., "onlineIccData": "<DE 55 hex>|null",
//       "emv": { "tvr": {"hex", "flags": [...]}, "tsi": {...}, "cvmResults": {"hex", "description"},
//                "tags": [{"tag", "name", "value"}] } } }
//   { "type": "error",  "code": "HOST_UNAVAILABLE|TRANSACTION_FAILED", "message": "..." }
#include "emv/domain.hpp"
#include "simulated/simulation_options.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace emv::agent {

struct SaleCommand {
    EntryMode entryMode{EntryMode::Insert};
    std::int64_t amountMinor{0};
    std::string transactionId;
    std::optional<std::string> kernel;
    simulated::SimulationOptions simulation;
};

// Throws std::invalid_argument with a client-facing message if the body is not a valid command.
SaleCommand parseSaleCommand(const std::string& body);

nlohmann::json stageEvent(TransactionStage stage, StageStatus status, const std::string& detail);
nlohmann::json resultEvent(const SaleCommand& command, const std::string& kernelName, const TransactionResult& result);
nlohmann::json errorEvent(const std::string& code, const std::string& message);

// Single-line JSON; invalid UTF-8 is replaced rather than thrown.
std::string toLine(const nlohmann::json& event);

} // namespace emv::agent
