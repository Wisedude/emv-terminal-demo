// Composition root: the only place that knows concrete adapters. Swapping the kernel,
// reader or host transport means changing the wiring here, never the core.
#include "emv/kernel/kernel_registry.hpp"
#include "emv/policy/preferred_aid_selector.hpp"
#include "emv/transaction_service.hpp"
#include "simulated/register_simulated_kernels.hpp"
#include "simulated/simulated_card_reader.hpp"
#include "simulated/stub_host_authorizer.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace emv;

constexpr int kExitApproved = 0;
constexpr int kExitDeclined = 1;
constexpr int kExitFailed = 2;

struct Options {
    std::string kernelName = simulated::kContactKernelName;
    bool listKernels = false;
};

Options parseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list-kernels") {
            options.listKernels = true;
        } else if (arg == "--kernel" && i + 1 < argc) {
            options.kernelName = argv[++i];
        } else {
            throw std::invalid_argument("Usage: emv_demo [--kernel <name>] [--list-kernels]");
        }
    }
    return options;
}

KernelRegistry buildKernelRegistry() {
    KernelRegistry registry;
    simulated::registerSimulatedKernels(registry);
    // Vendor kernels register here, e.g. vendor_x::registerKernels(registry);
    return registry;
}

EntryMode entryModeFor(const std::string& kernelName) {
    return kernelName == simulated::kContactlessKernelName ? EntryMode::Tap : EntryMode::Insert;
}

void printResult(const std::string& kernelName, const TransactionResult& result) {
    const std::string host = result.host
        ? result.host->responseCode + " / " + result.host->message
        : "Not requested";
    const std::string secondCryptogram = result.final
        ? toString(result.final->type) + " (" + result.final->opaqueCryptogram + ")"
        : "None (offline outcome or kernel has no second GENERATE AC)";

    std::cout << "\n=== Transaction result ===\n"
              << "Kernel: " << kernelName << "\n"
              << "Selected AID: " << result.selectedAid << "\n"
              << "First cryptogram: " << toString(result.first.type) << " (" << result.first.opaqueCryptogram << ")\n"
              << "Host response: " << host << "\n"
              << "Second cryptogram: " << secondCryptogram << "\n"
              << "Issuer scripts: " << result.issuerScriptStatus << "\n"
              << "Outcome: " << (result.approved ? "APPROVED" : "DECLINED") << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parseOptions(argc, argv);
        const KernelRegistry kernels = buildKernelRegistry();

        if (options.listKernels) {
            for (const auto& name : kernels.names()) std::cout << name << "\n";
            return kExitApproved;
        }

        const auto kernel = kernels.create(options.kernelName);
        simulated::SimulatedCardReader reader;
        PreferredAidSelector selector({"A0000000031010"});
        simulated::StubHostAuthorizer host;
        TransactionService service(TerminalConfig{"ZAR"}, reader, *kernel, selector, host);

        const auto result = service.runSale(entryModeFor(options.kernelName), 12500, "SIM-CPP-0001");
        printResult(options.kernelName, result);
        return result.approved ? kExitApproved : kExitDeclined;
    } catch (const std::exception& e) {
        std::cerr << "Transaction failed safely: " << e.what() << "\n";
        return kExitFailed;
    }
}
