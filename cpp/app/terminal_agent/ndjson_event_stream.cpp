#include "terminal_agent/ndjson_event_stream.hpp"
#include "terminal_agent/agent_protocol.hpp"
#include <thread>
#include <utility>

namespace emv::agent {

NdjsonEventStream::NdjsonEventStream(LineWriter writer, std::chrono::milliseconds stepDelay)
    : writer_(std::move(writer)), stepDelay_(stepDelay) {}

void NdjsonEventStream::onStage(TransactionStage stage, StageStatus status, const std::string& detail) {
    send(stageEvent(stage, status, detail));
    if (status == StageStatus::Started && stepDelay_.count() > 0) std::this_thread::sleep_for(stepDelay_);
}

void NdjsonEventStream::send(const nlohmann::json& event) {
    try {
        writer_(toLine(event));
    } catch (...) {
        // Observers must not throw into the transaction flow.
    }
}

} // namespace emv::agent
