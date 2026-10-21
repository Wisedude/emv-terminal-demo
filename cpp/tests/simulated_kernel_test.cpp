#include "emv/emv_tags.hpp"
#include "emv/policy/preferred_aid_selector.hpp"
#include "emv/transaction_service.hpp"
#include "simulated/mock_issuer_crypto.hpp"
#include "simulated/simulated_card_reader.hpp"
#include "simulated/simulated_contact_kernel.hpp"
#include "simulated/simulated_contactless_kernel.hpp"
#include "simulated/stub_host_authorizer.hpp"
#include "test_support.hpp"
#include <memory>

// End-to-end runs of TransactionService with the simulated kernels, the shipped config/ files and the
// simulated test cards.
namespace {

using namespace emv;
using namespace emv::simulated;
using namespace emv::test;

std::shared_ptr<const config::EmvConfiguration> configuration() {
    static const auto loaded =
        std::make_shared<const config::EmvConfiguration>(config::loadEmvConfiguration(EMV_CONFIG_DIR).configuration);
    return loaded;
}

// Wraps the stub issuer: counts calls, keeps the request, can decline or corrupt the ARPC.
class TestIssuer final : public IHostAuthorizer {
public:
    enum class Mode { Approve, Decline, BadArpc };
    explicit TestIssuer(Mode mode = Mode::Approve) : mode_(mode) {}

    HostResponse authorize(const HostRequest& request) override {
        ++calls;
        lastRequest = request;
        HostResponse response = StubHostAuthorizer().authorize(request);
        if (mode_ == Mode::Decline) {
            const Bytes arqc = parseTlv(request.iccData).front().value;  // 9F26 is sent first
            response = {false, "05", "", mockIssuerAuthenticationData(arqc, "05"), "Declined", std::nullopt};
        }
        if (mode_ == Mode::BadArpc) {
            auto& firstByte = (*response.issuerAuthenticationData)[0];
            firstByte = static_cast<std::uint8_t>(firstByte ^ 0xFF);
        }
        return response;
    }

    int calls = 0;
    HostRequest lastRequest;

private:
    Mode mode_;
};

struct Sale {
    TransactionResult result;
    RecordingObserver observer;
};

Sale runSale(SimulatedCardId card, EntryMode mode, std::int64_t amountMinor, IHostAuthorizer& host,
             CardBehaviour behaviour = CardBehaviour::Normal) {
    const SimulationOptions options{card, behaviour};
    std::unique_ptr<IEmvKernel> kernel;
    if (mode == EntryMode::Tap) {
        kernel = std::make_unique<SimulatedContactlessKernel>(configuration(), options);
    } else {
        kernel = std::make_unique<SimulatedContactKernel>(configuration(), options);
    }
    SimulatedCardReader reader(card);
    PreferredAidSelector selector({});
    TransactionService service(TerminalConfig{"ZAR"}, reader, *kernel, selector, host);
    Sale sale;
    sale.result = service.runSale(mode, amountMinor, "TEST-1", sale.observer);
    return sale;
}

Bytes tagValue(const TlvList& data, Tag tag) {
    for (const auto& field : data) {
        if (field.tag == tag) return field.value;
    }
    return {};
}

Bytes tvrOf(const Sale& sale) {
    return tagValue(sale.result.emvData, tags::TerminalVerificationResults);
}

Bytes tsiOf(const Sale& sale) {
    return tagValue(sale.result.emvData, tags::TransactionStatusInformation);
}

void visaContactGoesOnlineWithOnlinePin() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Insert, 12500, issuer);

    require(issuer.calls == 1 && sale.result.approved, "Visa contact sale goes online and is approved");
    require(sale.result.final && sale.result.final->type == CryptogramType::TC, "second GENERATE AC returns TC");
    require(hasFlag(tvrOf(sale), tvr::OnlinePinEntered), "TVR: online PIN entered");
    require(hasFlag(tvrOf(sale), tvr::ExceedsFloorLimit), "TVR: over the R0.00 floor limit in AIDS_CONTACT.TXT");
    require(hasFlag(tvrOf(sale), tvr::DifferentApplicationVersions), "TVR: card 008C vs configured 0000");
    require((tagValue(sale.result.emvData, tags::CvmResults) == hexToBytes("420300")), "CVM Results 42 03 00");
    for (const auto& flag : {tsi::OfflineDataAuthenticationPerformed, tsi::CardholderVerificationPerformed,
                             tsi::CardRiskManagementPerformed, tsi::TerminalRiskManagementPerformed,
                             tsi::IssuerAuthenticationPerformed, tsi::ScriptProcessingPerformed}) {
        require(hasFlag(tsiOf(sale), flag), std::string("TSI: ") + flag.description);
    }
    require((tagValue(sale.result.emvData, tags::IssuerScriptResults) == hexToBytes("2000000001")),
            "Issuer Script Results: script 00000001 succeeded");
    require(tagValue(sale.result.emvData, tags::Pan).empty(), "the PAN never leaves the kernel");

    const TlvList de55 = parseTlv(issuer.lastRequest.iccData);
    require((tagValue(de55, tags::AmountAuthorised) == hexToBytes("000000012500")), "DE 55 carries the amount");
    require(!tagValue(de55, tags::ApplicationCryptogram).empty() && !tagValue(de55, tags::TerminalVerificationResults).empty(),
            "DE 55 carries the ARQC and the TVR");
    require((tagValue(de55, tags::CryptogramInformationData) == Bytes{0x80}), "DE 55 CID says ARQC");
}

void unsupportedAidFailsSelection() {
    TestIssuer issuer;
    try {
        runSale(SimulatedCardId::JcbUnsupported, EntryMode::Insert, 12500, issuer);
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("Application selection failed") == 0, "selection must fail");
        require(issuer.calls == 0, "nothing reaches the host");
        return;
    }
    throw std::runtime_error("a card with no supported AID must fail application selection");
}

void mastercardUnderFloorLimitApprovesOffline() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Mastercard, EntryMode::Insert, 5000, issuer);
    require(sale.result.approved && sale.result.first.type == CryptogramType::TC, "R50 under the R100 floor: TC");
    require(issuer.calls == 0, "offline approval does not call the host");
    require((tvrOf(sale) == Bytes(5, 0x00)), "clean TVR");
    require((tagValue(sale.result.emvData, tags::CvmResults) == hexToBytes("5E0300")), "signature CVM");
}

void expiredCardSetsTvr() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::VisaExpired, EntryMode::Insert, 12500, issuer);
    require(hasFlag(tvrOf(sale), tvr::ExpiredApplication), "TVR: expired application");
}

void unknownCaKeyFailsCda() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::MastercardUnknownCaKey, EntryMode::Insert, 5000, issuer);
    require(hasFlag(tvrOf(sale), tvr::CdaFailed), "TVR: CDA failed without CA key F5");
    require(issuer.calls == 1, "TAC-Online includes CDA failed, so the sale goes online");
}

void contactlessUnderFloorApprovesOffline() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Tap, 12500, issuer);
    require(sale.result.approved && sale.result.first.type == CryptogramType::TC, "R125 under the R200 floor: TC");
    require(issuer.calls == 0, "no host call");
    require((tagValue(sale.result.emvData, tags::CvmResults) == hexToBytes("1F0002")), "no CVM under the CVM limit");
}

void contactlessOverFloorGoesOnlineWithoutSecondAc() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Tap, 30000, issuer);
    require(issuer.calls == 1 && sale.result.approved, "R300 goes online and is approved");
    require(!sale.result.final, "no second GENERATE AC on contactless");
    require(!hasFlag(tsiOf(sale), tsi::IssuerAuthenticationPerformed), "no issuer authentication: card has left");
}

void contactlessOverCvmLimitUsesOnlinePin() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Tap, 60000, issuer);
    require((tagValue(sale.result.emvData, tags::CvmResults) == hexToBytes("020000")), "online PIN over R500");
    require(hasFlag(tvrOf(sale), tvr::OnlinePinEntered), "TVR: online PIN entered");
}

void contactlessOverTransactionLimitIsRefused() {
    TestIssuer issuer;
    requireThrows<std::runtime_error>([&] { runSale(SimulatedCardId::Visa, EntryMode::Tap, 80000, issuer); },
                                      "R800 is over the R700 contactless transaction limit");
    require(issuer.calls == 0, "nothing reaches the host");
}

void badArpcFailsIssuerAuthentication() {
    TestIssuer issuer(TestIssuer::Mode::BadArpc);
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Insert, 12500, issuer);
    require(hasFlag(tvrOf(sale), tvr::IssuerAuthenticationFailed), "TVR: issuer authentication failed");
    require(!sale.result.approved && sale.result.final->type == CryptogramType::AAC,
            "the card declines even though the issuer approved");
    require(sale.observer.detailOf("SECOND_CARD_ACTION_ANALYSIS:COMPLETED").find("Issuer authentication failed") == 0,
            "the stage detail reports the failed issuer authentication");
}

void issuerDeclineEndsWithAac() {
    TestIssuer issuer(TestIssuer::Mode::Decline);
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Insert, 12500, issuer);
    require(!sale.result.approved && sale.result.final->type == CryptogramType::AAC, "issuer decline: AAC");
    require(hasFlag(tsiOf(sale), tsi::IssuerAuthenticationPerformed), "ARPC on a decline is still verified");
    require(!hasFlag(tvrOf(sale), tvr::IssuerAuthenticationFailed), "and passes");
    require(!sale.result.issuerScriptsProcessed, "no scripts on a decline");
}

void decliningCardReturnsAacOffline() {
    TestIssuer issuer;
    const Sale sale = runSale(SimulatedCardId::Visa, EntryMode::Insert, 12500, issuer, CardBehaviour::Decline);
    require(!sale.result.approved && sale.result.first.type == CryptogramType::AAC, "card declines with AAC");
    require(issuer.calls == 0, "no host call");
}

} // namespace

int main() {
    return runTests("SimulatedKernel", {
        {"visaContactGoesOnlineWithOnlinePin", visaContactGoesOnlineWithOnlinePin},
        {"unsupportedAidFailsSelection", unsupportedAidFailsSelection},
        {"mastercardUnderFloorLimitApprovesOffline", mastercardUnderFloorLimitApprovesOffline},
        {"expiredCardSetsTvr", expiredCardSetsTvr},
        {"unknownCaKeyFailsCda", unknownCaKeyFailsCda},
        {"contactlessUnderFloorApprovesOffline", contactlessUnderFloorApprovesOffline},
        {"contactlessOverFloorGoesOnlineWithoutSecondAc", contactlessOverFloorGoesOnlineWithoutSecondAc},
        {"contactlessOverCvmLimitUsesOnlinePin", contactlessOverCvmLimitUsesOnlinePin},
        {"contactlessOverTransactionLimitIsRefused", contactlessOverTransactionLimitIsRefused},
        {"badArpcFailsIssuerAuthentication", badArpcFailsIssuerAuthentication},
        {"issuerDeclineEndsWithAac", issuerDeclineEndsWithAac},
        {"decliningCardReturnsAacOffline", decliningCardReturnsAacOffline},
    });
}
