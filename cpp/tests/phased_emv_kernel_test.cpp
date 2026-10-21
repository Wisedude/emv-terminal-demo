#include "emv/kernel/phased_emv_kernel.hpp"
#include "test_support.hpp"

namespace {

using namespace emv;
using namespace emv::test;

class RecordingPhasedKernel final : public PhasedEmvKernel {
public:
    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override { return card.applications; }
    OnlineCompletion completeOnlineTransaction(const HostResponse&) override { return {}; }
    TlvList transactionData() const override { return {}; }

    std::vector<std::string> phases;

private:
    std::string record(const char* phase) {
        phases.push_back(phase);
        return std::string("did ") + phase;
    }

    std::string initiateApplicationProcessing(const KernelTransaction&) override { return record("initiate"); }
    std::string readApplicationData(const KernelTransaction&) override { return record("read"); }
    std::string checkProcessingRestrictions(const KernelTransaction&) override { return record("restrictions"); }
    std::string performOfflineDataAuthentication(const KernelTransaction&) override { return record("oda"); }
    std::string validateCertificates(const KernelTransaction&) override { return record("certificates"); }
    std::string performCardholderVerification(const KernelTransaction&) override { return record("cvm"); }
    std::string performTerminalRiskManagement(const KernelTransaction&) override { return record("trm"); }
    std::string performTerminalActionAnalysis(const KernelTransaction&) override { return record("taa"); }
    FirstCryptogram firstGenerateAc(const KernelTransaction&) override {
        phases.push_back("generate-ac");
        return {CryptogramType::ARQC, "RECORDED"};
    }
};

void runsAllPhasesInEmvOrder() {
    RecordingPhasedKernel kernel;
    IEmvKernel& port = kernel;
    NullTransactionObserver observer;
    const auto first = port.processUntilFirstCryptogram(KernelTransaction{}, observer);

    const std::vector<std::string> expected{
        "initiate", "read", "restrictions", "oda", "certificates", "cvm", "trm", "taa", "generate-ac"};
    require(kernel.phases == expected, "phases must run once each, in EMV Book 3 order");
    require(first.type == CryptogramType::ARQC, "result of first GENERATE AC must be returned");
}

void reportsStartAndCompletionOfEveryPhase() {
    RecordingPhasedKernel kernel;
    RecordingObserver observer;
    kernel.processUntilFirstCryptogram(KernelTransaction{}, observer);

    require(observer.events.size() == 18, "each of the 9 phases must report STARTED and COMPLETED");
    require(observer.events.front() == "INITIATE_APPLICATION_PROCESSING:STARTED", "first event must start stage 2");
    require(observer.events.back() == "FIRST_CARD_ACTION_ANALYSIS:COMPLETED", "last event must complete stage 10");
    require(observer.details[1] == "did initiate", "a phase's returned detail must reach the observer");
}

} // namespace

int main() {
    return runTests("PhasedEmvKernel", {
        {"runsAllPhasesInEmvOrder", runsAllPhasesInEmvOrder},
        {"reportsStartAndCompletionOfEveryPhase", reportsStartAndCompletionOfEveryPhase},
    });
}
