#include "emv/transaction_service.hpp"
#include "test_support.hpp"

namespace {

using namespace emv;
using namespace emv::test;

const TerminalConfig kConfig{"ZAR"};

struct Fixture {
    explicit Fixture(CryptogramType firstOutcome, bool hostApproves = true, bool hasSecondGenerateAc = true)
        : kernel(firstOutcome, hasSecondGenerateAc), host(hostApproves), service(kConfig, reader, kernel, selector, host) {}

    TransactionResult sale() { return service.runSale(EntryMode::Insert, 12500, "TEST-TRANSACTION"); }

    FakeReader reader;
    FakeKernel kernel;
    FirstCandidateSelector selector;
    FakeHost host;
    TransactionService service;
};

void arqcGoesOnlineAndApprovesOnTc() {
    Fixture f(CryptogramType::ARQC);
    const auto result = f.sale();

    require(result.approved, "issuer approval + TC must approve");
    require(f.host.calls == 1, "host must be called once for ARQC");
    require(f.kernel.onlineCompletions == 1, "kernel must complete the online transaction");
    require(result.host.has_value(), "host response must be recorded");
    require(result.final.has_value() && result.final->type == CryptogramType::TC, "second cryptogram must be TC");
    require(result.issuerScriptsProcessed, "issuer scripts must be processed on the online path");
}

void tcApprovesOfflineWithoutHost() {
    Fixture f(CryptogramType::TC);
    const auto result = f.sale();

    require(result.approved, "TC must approve offline");
    require(f.host.calls == 0 && f.kernel.onlineCompletions == 0, "offline outcome must not go online");
    require(!result.host && !result.final && !result.issuerScriptsProcessed, "online-only fields must be empty");
}

void aacDeclinesOfflineWithoutHost() {
    Fixture f(CryptogramType::AAC);
    const auto result = f.sale();

    require(!result.approved, "AAC must decline offline");
    require(f.host.calls == 0 && f.kernel.onlineCompletions == 0, "offline outcome must not go online");
}

void issuerDeclineDeclines() {
    Fixture f(CryptogramType::ARQC, /*hostApproves=*/false);
    const auto result = f.sale();

    require(!result.approved, "issuer decline must decline");
    require(result.final.has_value() && result.final->type == CryptogramType::AAC, "kernel must finish with AAC");
}

void kernelWithoutSecondGenerateAcFollowsIssuer() {
    Fixture f(CryptogramType::ARQC, /*hostApproves=*/true, /*hasSecondGenerateAc=*/false);
    const auto result = f.sale();

    require(result.approved, "contactless-style kernel must follow the issuer decision");
    require(!result.final.has_value(), "no second cryptogram is expected");
}

void hostRequestCarriesTransactionContext() {
    Fixture f(CryptogramType::ARQC);
    f.sale();

    const HostRequest& request = f.host.lastRequest;
    require(request.currencyCode == "ZAR", "currency must come from terminal config");
    require(request.amountMinor == 12500, "amount must be forwarded");
    require(request.aid == "A0000000031010", "selected AID must be forwarded");
    require(request.cryptogramType == CryptogramType::ARQC, "cryptogram type must be forwarded");
    require(request.opaqueCryptogram == "SIMULATED-FIRST", "opaque ARQC must be forwarded");
    require(request.cardToken == "SIMULATED-CARD-01", "card token must be forwarded");
    require(f.kernel.lastTransaction.application.aid == "A0000000031010", "kernel must receive the selected application");
    require((request.iccData == Bytes{0x9F, 0x26, 0x01, 0xAA}), "kernel's DE 55 ICC data must be sent to the host");
}

void resultCarriesKernelEmvData() {
    Fixture online(CryptogramType::ARQC);
    Fixture offline(CryptogramType::TC);
    require(online.sale().emvData.size() == 1, "online result must carry the kernel's EMV data");
    require(offline.sale().emvData.size() == 1, "offline result must carry the kernel's EMV data");
}

void unsupportedAidFailsApplicationSelection() {
    FakeReader emptyReader({});
    FakeKernel kernel(CryptogramType::TC);
    FirstCandidateSelector selector;
    FakeHost host;
    TransactionService service(kConfig, emptyReader, kernel, selector, host);
    try {
        service.runSale(EntryMode::Insert, 100, "T1");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("Application selection failed") == 0,
                "error must say application selection failed");
        return;
    }
    throw std::runtime_error("no mutually supported AID must fail");
}

void rejectsInvalidInput() {
    Fixture f(CryptogramType::TC);
    requireThrows<std::invalid_argument>([&] { f.service.runSale(EntryMode::Insert, 0, "T1"); },
                                         "zero amount must be rejected");
    requireThrows<std::invalid_argument>([&] { f.service.runSale(EntryMode::Insert, 100, ""); },
                                         "empty transaction ID must be rejected");
    require(f.kernel.firstCryptogramRequests == 0, "kernel must not run for invalid input");
}

void failsWhenNoApplicationsOrSelectionCancelled() {
    FakeReader emptyReader({});
    FakeKernel kernel(CryptogramType::TC);
    FirstCandidateSelector selector;
    FakeHost host;
    TransactionService noApps(kConfig, emptyReader, kernel, selector, host);
    requireThrows<std::runtime_error>([&] { noApps.runSale(EntryMode::Insert, 100, "T1"); },
                                      "empty candidate list must fail");

    FakeReader reader;
    CancellingSelector cancelling;
    TransactionService cancelled(kConfig, reader, kernel, cancelling, host);
    requireThrows<std::runtime_error>([&] { cancelled.runSale(EntryMode::Insert, 100, "T1"); },
                                      "cancelled selection must fail");
    require(kernel.firstCryptogramRequests == 0, "kernel processing must not start without a selected application");
}

void reportsServiceStagesOnOnlinePath() {
    Fixture f(CryptogramType::ARQC);
    RecordingObserver observer;
    f.service.runSale(EntryMode::Insert, 12500, "TEST-TRANSACTION", observer);

    const std::vector<std::string> expected{
        "APPLICATION_SELECTION:STARTED", "APPLICATION_SELECTION:COMPLETED",
        "ONLINE_AUTHORIZATION:STARTED", "ONLINE_AUTHORIZATION:COMPLETED",
        "SECOND_CARD_ACTION_ANALYSIS:STARTED", "SECOND_CARD_ACTION_ANALYSIS:COMPLETED",
        "ISSUER_SCRIPT_PROCESSING:COMPLETED"};
    require(observer.events == expected, "service must report its stages in order on the online path");
}

void reportsOnlineStagesSkippedWhenOffline() {
    Fixture f(CryptogramType::TC);
    RecordingObserver observer;
    f.service.runSale(EntryMode::Insert, 12500, "TEST-TRANSACTION", observer);

    require(observer.saw("ONLINE_AUTHORIZATION:SKIPPED"), "online authorization must be reported skipped");
    require(observer.saw("SECOND_CARD_ACTION_ANALYSIS:SKIPPED"), "second card action analysis must be reported skipped");
    require(observer.saw("ISSUER_SCRIPT_PROCESSING:SKIPPED"), "issuer scripts must be reported skipped");
}

void reportsMissingSecondGenerateAcAsSkipped() {
    Fixture f(CryptogramType::ARQC, /*hostApproves=*/true, /*hasSecondGenerateAc=*/false);
    RecordingObserver observer;
    f.service.runSale(EntryMode::Tap, 12500, "TEST-TRANSACTION", observer);

    require(observer.saw("SECOND_CARD_ACTION_ANALYSIS:SKIPPED"), "absent second GENERATE AC must be reported skipped");
}

void unreachableHostNeverApproves() {
    FakeReader reader;
    FakeKernel kernel(CryptogramType::ARQC);
    FirstCandidateSelector selector;
    UnreachableHost host;
    TransactionService service(kConfig, reader, kernel, selector, host);

    requireThrows<HostUnavailableError>([&] { service.runSale(EntryMode::Insert, 100, "T1"); },
                                        "host failure must surface as HostUnavailableError");
    require(kernel.onlineCompletions == 0, "kernel must not complete online without an issuer response");
}

} // namespace

int main() {
    return runTests("TransactionService", {
        {"arqcGoesOnlineAndApprovesOnTc", arqcGoesOnlineAndApprovesOnTc},
        {"tcApprovesOfflineWithoutHost", tcApprovesOfflineWithoutHost},
        {"aacDeclinesOfflineWithoutHost", aacDeclinesOfflineWithoutHost},
        {"issuerDeclineDeclines", issuerDeclineDeclines},
        {"kernelWithoutSecondGenerateAcFollowsIssuer", kernelWithoutSecondGenerateAcFollowsIssuer},
        {"hostRequestCarriesTransactionContext", hostRequestCarriesTransactionContext},
        {"rejectsInvalidInput", rejectsInvalidInput},
        {"failsWhenNoApplicationsOrSelectionCancelled", failsWhenNoApplicationsOrSelectionCancelled},
        {"reportsServiceStagesOnOnlinePath", reportsServiceStagesOnOnlinePath},
        {"reportsOnlineStagesSkippedWhenOffline", reportsOnlineStagesSkippedWhenOffline},
        {"reportsMissingSecondGenerateAcAsSkipped", reportsMissingSecondGenerateAcAsSkipped},
        {"unreachableHostNeverApproves", unreachableHostNeverApproves},
        {"resultCarriesKernelEmvData", resultCarriesKernelEmvData},
        {"unsupportedAidFailsApplicationSelection", unsupportedAidFailsApplicationSelection},
    });
}
