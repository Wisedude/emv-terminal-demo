#pragma once
#include "common/kernel_catalog.hpp"
#include "emv/domain.hpp"
#include "emv/ports/emv_kernel.hpp"
#include "terminal_agent/agent_protocol.hpp"
#include "terminal_agent/ndjson_event_stream.hpp"
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace httplib {
class Server;
struct Request;
struct Response;
} // namespace httplib

namespace emv::agent {

struct AgentConfig {
    app::LoadedConfiguration configuration;
    std::string hostBaseUrl;
    TerminalConfig terminal;
    std::vector<std::string> preferredAids;
    std::chrono::milliseconds stepDelay{0};
};

// Local HTTP front door of the terminal: the UI starts sales here, and the agent runs them
// through TransactionService with the real host authorizer. One transaction at a time,
// because a terminal has one card reader.
class TerminalAgent {
public:
    explicit TerminalAgent(AgentConfig config);
    void registerRoutes(httplib::Server& server);

private:
    void handleHealth(const httplib::Request& request, httplib::Response& response) const;
    void handleSale(const httplib::Request& request, httplib::Response& response);
    void runSale(const SaleCommand& command, const std::string& kernelName, IEmvKernel& kernel,
                 NdjsonEventStream& events) const;

    AgentConfig config_;
    std::atomic<bool> busy_{false};
};

} // namespace emv::agent
