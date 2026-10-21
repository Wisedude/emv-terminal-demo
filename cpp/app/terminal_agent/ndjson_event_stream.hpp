#pragma once
#include "emv/ports/transaction_observer.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <functional>
#include <string>

namespace emv::agent {

// Writes transaction progress to the HTTP response as NDJSON lines, as it happens.
class NdjsonEventStream final : public ITransactionObserver {
public:
    // Write failures (client went away) are ignored: the card transaction must still run to completion.
    using LineWriter = std::function<void(const std::string& line)>;

    // stepDelay paces STARTED events so a human can follow the demo; use zero for real terminals.
    NdjsonEventStream(LineWriter writer, std::chrono::milliseconds stepDelay);

    void onStage(TransactionStage stage, StageStatus status, const std::string& detail) override;
    void send(const nlohmann::json& event);

private:
    LineWriter writer_;
    std::chrono::milliseconds stepDelay_;
};

} // namespace emv::agent
