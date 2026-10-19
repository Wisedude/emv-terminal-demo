#include "emv/kernel/phased_emv_kernel.hpp"
#include "test_support.hpp"

namespace {

using namespace emv;
using namespace emv::test;

class RecordingPhasedKernel final : public PhasedEmvKernel {
public:
    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override { return card.applications; }
    OnlineCompletion completeOnlineTransaction(const HostResponse&) override { return {}; }

    std::vector<std::string> phases;

private:
    void initiateApplicationProcessing(const KernelTransaction&) override { phases.push_back("initiate"); }
    void readApplicationData(const KernelTransaction&) override { phases.push_back("read"); }
    void checkProcessingRestrictions(const KernelTransaction&) override { phases.push_back("restrictions"); }
    void performOfflineDataAuthentication(const KernelTransaction&) override { phases.push_back("oda"); }
    void validateCertificates(const KernelTransaction&) override { phases.push_back("certificates"); }
    void performCardholderVerification(const KernelTransaction&) override { phases.push_back("cvm"); }
    void performTerminalRiskManagement(const KernelTransaction&) override { phases.push_back("trm"); }
    void performTerminalActionAnalysis(const KernelTransaction&) override { phases.push_back("taa"); }
    FirstCryptogram firstGenerateAc(const KernelTransaction&) override {
        phases.push_back("generate-ac");
        return {CryptogramType::ARQC, "RECORDED"};
    }
};

void runsAllPhasesInEmvOrder() {
    RecordingPhasedKernel kernel;
    IEmvKernel& port = kernel;
    const auto first = port.processUntilFirstCryptogram(KernelTransaction{});

    const std::vector<std::string> expected{
        "initiate", "read", "restrictions", "oda", "certificates", "cvm", "trm", "taa", "generate-ac"};
    require(kernel.phases == expected, "phases must run once each, in EMV Book 3 order");
    require(first.type == CryptogramType::ARQC, "result of first GENERATE AC must be returned");
}

} // namespace

int main() {
    return runTests("PhasedEmvKernel", {
        {"runsAllPhasesInEmvOrder", runsAllPhasesInEmvOrder},
    });
}
