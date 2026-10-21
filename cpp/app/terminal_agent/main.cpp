// Terminal agent composition root: serves the local HTTP API the UI uses to start sales.
#include "common/kernel_catalog.hpp"
#include "terminal_agent/terminal_agent.hpp"
#include <httplib.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* kUsage =
    "Usage: emv_terminal_agent [--bind 127.0.0.1] [--port 9090] [--host-url http://localhost:8080] "
    "[--currency ZAR] [--step-delay-ms 250] [--config-dir <folder with TERMINAL.TXT, AIDS_*.TXT, CAKEYS.TXT>]";

struct Options {
    std::string bindAddress = "127.0.0.1";
    int port = 9090;
    std::filesystem::path configDirectory = emv::app::defaultConfigDirectory();
    std::string hostBaseUrl = "http://localhost:8080";
    std::string currencyCode = "ZAR";
    std::chrono::milliseconds stepDelay{250};
};

Options parseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (i + 1 >= argc) throw std::invalid_argument(kUsage);
        const std::string value = argv[++i];
        if (arg == "--bind") {
            options.bindAddress = value;
        } else if (arg == "--port") {
            options.port = std::stoi(value);
        } else if (arg == "--host-url") {
            options.hostBaseUrl = value;
        } else if (arg == "--currency") {
            options.currencyCode = value;
        } else if (arg == "--step-delay-ms") {
            options.stepDelay = std::chrono::milliseconds(std::stoi(value));
        } else if (arg == "--config-dir") {
            options.configDirectory = value;
        } else {
            throw std::invalid_argument(kUsage);
        }
    }
    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parseOptions(argc, argv);
        emv::agent::AgentConfig config;
        config.configuration = emv::app::loadConfiguration(options.configDirectory);
        config.hostBaseUrl = options.hostBaseUrl;
        config.terminal = emv::TerminalConfig{options.currencyCode};
        config.preferredAids = {"A0000000031010"};
        config.stepDelay = options.stepDelay;

        emv::agent::TerminalAgent agent(config);
        httplib::Server server;
        agent.registerRoutes(server);

        std::cout << "EMV terminal agent listening on http://" << options.bindAddress << ":" << options.port << "\n"
                  << "Authorizing against " << options.hostBaseUrl << "\n";
        if (!server.listen(options.bindAddress, options.port)) {
            std::cerr << "Could not listen on " << options.bindAddress << ":" << options.port << "\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Terminal agent failed to start: " << e.what() << "\n";
        return 2;
    }
}
