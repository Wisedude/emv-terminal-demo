// Command-line composition root: runs one simulated sale and prints the result.
// Swapping the kernel, reader or host transport means changing the wiring here, never the core.
#include "common/kernel_catalog.hpp"
#include "emv/emv_tags.hpp"
#include "emv/policy/preferred_aid_selector.hpp"
#include "emv/transaction_service.hpp"
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
constexpr const char* kUsage =
    "Usage: emv_demo [--kernel <name>] [--entry Tap|Insert|Swipe|Manual] [--amount <minor units>]\n"
    "                [--card VISA|MASTERCARD|VISA_EXPIRED|MASTERCARD_UNKNOWN_CA_KEY|JCB_UNSUPPORTED]\n"
    "                [--card-behaviour NORMAL|FORCE_ONLINE|DECLINE] [--config-dir <folder>] [--list-kernels]";

struct Options {
    std::string kernelName;  // empty: default kernel for the entry mode
    EntryMode entryMode = EntryMode::Insert;
    std::int64_t amountMinor = 12500;
    std::filesystem::path configDirectory = app::defaultConfigDirectory();
    simulated::SimulationOptions simulation;
    bool listKernels = false;
};

template <typename Value>
Value require(std::optional<Value> value) {
    if (!value) throw std::invalid_argument(kUsage);
    return *value;
}

Options parseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list-kernels") {
            options.listKernels = true;
            continue;
        }
        if (i + 1 >= argc) throw std::invalid_argument(kUsage);
        const std::string value = argv[++i];
        if (arg == "--kernel") {
            options.kernelName = value;
        } else if (arg == "--entry") {
            options.entryMode = require(entryModeFromString(value));
        } else if (arg == "--amount") {
            options.amountMinor = std::stoll(value);
        } else if (arg == "--card") {
            options.simulation.card = require(simulated::simulatedCardFromString(value));
        } else if (arg == "--card-behaviour") {
            options.simulation.cardBehaviour = require(simulated::cardBehaviourFromString(value));
        } else if (arg == "--config-dir") {
            options.configDirectory = value;
        } else {
            throw std::invalid_argument(kUsage);
        }
    }
    if (options.kernelName.empty()) options.kernelName = app::defaultKernelFor(options.entryMode);
    return options;
}

const Bytes* findTag(const TlvList& data, Tag tag) {
    for (const auto& field : data) {
        if (field.tag == tag) return &field.value;
    }
    return nullptr;
}

void printFlags(const char* name, const TlvList& data, Tag tag, std::vector<std::string> (*describe)(const Bytes&)) {
    const Bytes* value = findTag(data, tag);
    std::cout << name << ": " << (value ? toHex(*value) : "-") << "\n";
    if (value) {
        for (const auto& flag : describe(*value)) std::cout << "    - " << flag << "\n";
    }
}

void printResult(const std::string& kernelName, const TransactionResult& result) {
    const std::string host = result.host
        ? result.host->responseCode + " / " + result.host->message
        : "Not requested";
    const std::string secondCryptogram = result.final
        ? toString(result.final->type) + " (" + result.final->opaqueCryptogram + ")"
        : "None (offline outcome or kernel has no second GENERATE AC)";
    const Bytes* cvm = findTag(result.emvData, tags::CvmResults);

    std::cout << "\n=== Transaction result ===\n"
              << "Kernel: " << kernelName << "\n"
              << "Selected AID: " << result.selectedAid << "\n"
              << "First cryptogram: " << toString(result.first.type) << " (" << result.first.opaqueCryptogram << ")\n"
              << "Host response: " << host << "\n"
              << "Second cryptogram: " << secondCryptogram << "\n"
              << "Issuer scripts: " << result.issuerScriptStatus << "\n";
    printFlags("TVR", result.emvData, tags::TerminalVerificationResults, describeTvr);
    printFlags("TSI", result.emvData, tags::TransactionStatusInformation, describeTsi);
    std::cout << "CVM Results: " << (cvm ? toHex(*cvm) + " (" + describeCvmResults(*cvm) + ")" : "-") << "\n";
    if (result.host) std::cout << "DE 55 sent to host: " << toHex(result.first.iccData) << "\n";
    std::cout << "Outcome: " << (result.approved ? "APPROVED" : "DECLINED") << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parseOptions(argc, argv);
        const app::LoadedConfiguration configuration = app::loadConfiguration(options.configDirectory);
        const KernelRegistry kernels = app::buildKernelRegistry(configuration.emv, options.simulation);

        if (options.listKernels) {
            for (const auto& name : kernels.names()) std::cout << name << "\n";
            return kExitApproved;
        }

        const auto kernel = kernels.create(options.kernelName);
        simulated::SimulatedCardReader reader(options.simulation.card);
        PreferredAidSelector selector({"A0000000031010"});
        simulated::StubHostAuthorizer host;
        TransactionService service(TerminalConfig{"ZAR"}, reader, *kernel, selector, host);

        const auto result = service.runSale(options.entryMode, options.amountMinor, "SIM-CPP-0001");
        printResult(options.kernelName, result);
        return result.approved ? kExitApproved : kExitDeclined;
    } catch (const std::exception& e) {
        std::cerr << "Transaction failed safely: " << e.what() << "\n";
        return kExitFailed;
    }
}
