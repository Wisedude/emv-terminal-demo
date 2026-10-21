#include "terminal_agent/terminal_agent.hpp"
#include "common/kernel_catalog.hpp"
#include "emv/policy/preferred_aid_selector.hpp"
#include "emv/transaction_service.hpp"
#include "http/http_host_authorizer.hpp"
#include "simulated/simulated_card_reader.hpp"
#include <httplib.h>
#include <iostream>
#include <memory>
#include <utility>

namespace emv::agent {
namespace {

constexpr const char* kJson = "application/json";

void sendJson(httplib::Response& response, int status, const nlohmann::json& body) {
    response.status = status;
    response.set_content(body.dump(), kJson);
}

void sendError(httplib::Response& response, int status, const std::string& message) {
    sendJson(response, status, {{"message", message}});
}

std::vector<std::string> simulatedCardNames() {
    std::vector<std::string> names;
    for (const auto card : simulated::allSimulatedCards()) names.push_back(simulated::toString(card));
    return names;
}

// Clears the busy flag when the response is finished, however it finishes.
class BusyGuard {
public:
    explicit BusyGuard(std::atomic<bool>& busy) : busy_(busy) {}
    ~BusyGuard() { busy_ = false; }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;

private:
    std::atomic<bool>& busy_;
};

} // namespace

TerminalAgent::TerminalAgent(AgentConfig config) : config_(std::move(config)) {}

void TerminalAgent::registerRoutes(httplib::Server& server) {
    server.Get("/terminal/health", [this](const httplib::Request& req, httplib::Response& res) { handleHealth(req, res); });
    server.Post("/terminal/transactions", [this](const httplib::Request& req, httplib::Response& res) { handleSale(req, res); });
}

void TerminalAgent::handleHealth(const httplib::Request&, httplib::Response& response) const {
    sendJson(response, 200, {
        {"status", "UP"},
        {"busy", busy_.load()},
        {"hostUrl", config_.hostBaseUrl},
        {"kernels", app::buildKernelRegistry(config_.configuration.emv).names()},
        {"cards", simulatedCardNames()},
        {"configuration", {
            {"directory", config_.configuration.directory.string()},
            {"contactAids", config_.configuration.emv->contactAids.size()},
            {"contactlessAids", config_.configuration.emv->contactlessAids.size()},
            {"caPublicKeys", config_.configuration.emv->caPublicKeys.size()},
            {"warnings", config_.configuration.warnings},
        }},
    });
}

void TerminalAgent::handleSale(const httplib::Request& request, httplib::Response& response) {
    SaleCommand command;
    try {
        command = parseSaleCommand(request.body);
    } catch (const std::invalid_argument& error) {
        sendError(response, 400, error.what());
        return;
    }

    const std::string kernelName = command.kernel.value_or(app::defaultKernelFor(command.entryMode));
    const KernelRegistry kernels = app::buildKernelRegistry(config_.configuration.emv, command.simulation);
    if (!kernels.contains(kernelName)) {
        sendError(response, 400, "Unknown kernel '" + kernelName + "'");
        return;
    }

    bool idle = false;
    if (!busy_.compare_exchange_strong(idle, true)) {
        sendError(response, 409, "Terminal is busy with another transaction");
        return;
    }
    auto busyGuard = std::make_shared<BusyGuard>(busy_);
    std::shared_ptr<IEmvKernel> kernel = kernels.create(kernelName);

    std::cout << "\n[Agent] Sale " << command.transactionId << ": " << command.amountMinor << " minor, "
              << toString(command.entryMode) << ", kernel=" << kernelName << ", card="
              << simulated::toString(command.simulation.card) << "\n";

    response.set_header("Cache-Control", "no-cache");
    response.set_chunked_content_provider(
        "application/x-ndjson",
        [this, command, kernelName, kernel, busyGuard](std::size_t, httplib::DataSink& sink) {
            NdjsonEventStream events([&sink](const std::string& line) { sink.write(line.data(), line.size()); },
                                     config_.stepDelay);
            runSale(command, kernelName, *kernel, events);
            sink.done();
            return true;
        });
}

void TerminalAgent::runSale(const SaleCommand& command, const std::string& kernelName, IEmvKernel& kernel,
                            NdjsonEventStream& events) const {
    try {
        simulated::SimulatedCardReader reader(command.simulation.card);
        PreferredAidSelector selector(config_.preferredAids);
        http::HttpHostAuthorizer host({config_.hostBaseUrl});
        TransactionService service(config_.terminal, reader, kernel, selector, host);

        const TransactionResult result =
            service.runSale(command.entryMode, command.amountMinor, command.transactionId, events);
        std::cout << "[Agent] Sale " << command.transactionId << " " << (result.approved ? "APPROVED" : "DECLINED") << "\n";
        events.send(resultEvent(command, kernelName, result));
    } catch (const HostUnavailableError& error) {
        std::cerr << "[Agent] Sale " << command.transactionId << " host unavailable: " << error.what() << "\n";
        events.send(errorEvent("HOST_UNAVAILABLE", error.what()));
    } catch (const std::exception& error) {
        std::cerr << "[Agent] Sale " << command.transactionId << " failed: " << error.what() << "\n";
        events.send(errorEvent("TRANSACTION_FAILED", error.what()));
    }
}

} // namespace emv::agent
