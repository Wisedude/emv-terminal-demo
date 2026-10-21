#include "simulated/simulated_emv_engine.hpp"
#include "simulated/mock_issuer_crypto.hpp"
#include <atomic>
#include <ctime>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

namespace emv::simulated {
namespace {

// Sent to the host in DE 55, in this order, when present.
constexpr Tag kOnlineDataTags[] = {
    tags::ApplicationCryptogram, tags::CryptogramInformationData, tags::IssuerApplicationData,
    tags::UnpredictableNumber, tags::ApplicationTransactionCounter, tags::TerminalVerificationResults,
    tags::TransactionDate, tags::TransactionType, tags::AmountAuthorised, tags::TransactionCurrencyCode,
    tags::ApplicationInterchangeProfile, tags::TerminalCountryCode, tags::CvmResults, tags::TerminalCapabilities,
    tags::TerminalType, tags::InterfaceDeviceSerialNumber, tags::DedicatedFileName,
    tags::ApplicationVersionTerminal, tags::AmountOther, tags::PanSequenceNumber, tags::TransactionSequenceCounter,
    tags::TerminalTransactionQualifiers};

// Shown on screen and receipts. Deliberately excludes the PAN and track 2.
constexpr Tag kDisplayTags[] = {
    tags::DedicatedFileName, tags::ApplicationLabel, tags::ApplicationInterchangeProfile,
    tags::TerminalVerificationResults, tags::TransactionStatusInformation, tags::CvmResults,
    tags::CryptogramInformationData, tags::ApplicationCryptogram, tags::IssuerApplicationData,
    tags::ApplicationTransactionCounter, tags::UnpredictableNumber, tags::TransactionDate, tags::TransactionTime,
    tags::TransactionType, tags::AmountAuthorised, tags::TransactionCurrencyCode, tags::TerminalCountryCode,
    tags::TerminalCapabilities, tags::TerminalType, tags::ApplicationVersionTerminal, tags::ApplicationVersionCard,
    tags::PanSequenceNumber, tags::AuthorisationResponseCode, tags::IssuerAuthenticationData,
    tags::IssuerScriptResults};

// Set by GET PROCESSING OPTIONS or by the terminal, so not copied again by READ RECORD.
constexpr Tag kNotReadFromRecords[] = {tags::ApplicationInterchangeProfile, tags::ApplicationFileLocator,
                                       tags::ApplicationTransactionCounter, tags::LastOnlineAtc};

std::uint64_t binaryValue(const Bytes& bytes) {
    std::uint64_t value = 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return value;
}

std::int64_t bcdValue(const Bytes& bytes) {
    std::int64_t value = 0;
    for (const auto byte : bytes) value = value * 100 + (byte >> 4) * 10 + (byte & 0x0F);
    return value;
}

Bytes toBcd(std::uint64_t value, std::size_t length) {
    Bytes out(length, 0x00);
    for (std::size_t i = length; i-- > 0;) {
        out[i] = static_cast<std::uint8_t>((value % 10) | (((value / 10) % 10) << 4));
        value /= 100;
    }
    return out;
}

Bytes counterBytes(std::uint16_t value) {
    return {static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value & 0xFF)};
}

std::uint8_t byteAt(const Bytes& bytes, std::size_t index) {
    return index < bytes.size() ? bytes[index] : std::uint8_t{0};
}

std::string formatAmount(std::int64_t minor) {
    const std::int64_t cents = minor % 100;
    return "R" + std::to_string(minor / 100) + (cents < 10 ? ".0" : ".") + std::to_string(cents);
}

std::string maskedPan(const Bytes& pan) {
    std::string digits = toHex(pan);
    while (!digits.empty() && digits.back() == 'F') digits.pop_back();
    return "****" + (digits.size() > 4 ? digits.substr(digits.size() - 4) : digits);
}

std::string join(const std::vector<std::string>& parts, const char* separator) {
    std::string text;
    for (const auto& part : parts) text += (text.empty() ? "" : separator) + part;
    return text;
}

struct Clock {
    Bytes date;  // YYMMDD, BCD
    Bytes time;  // HHMMSS, BCD
};

Clock now() {
    const std::time_t seconds = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    const auto date = static_cast<std::uint64_t>((local.tm_year % 100) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday);
    const auto time = static_cast<std::uint64_t>(local.tm_hour * 10000 + local.tm_min * 100 + local.tm_sec);
    return {toBcd(date, 3), toBcd(time, 3)};
}

Bytes unpredictableNumber() {
    static thread_local std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<int> byte(0, 255);
    Bytes number(4);
    for (auto& value : number) value = static_cast<std::uint8_t>(byte(generator));
    return number;
}

std::uint32_t nextTransactionSequenceCounter() {
    static std::atomic<std::uint32_t> counter{0};
    return ++counter;
}

std::uint8_t cryptogramInformationData(CryptogramType type) {
    switch (type) {
        case CryptogramType::AAC: return 0x00;
        case CryptogramType::TC: return 0x40;
        case CryptogramType::ARQC: return 0x80;
    }
    return 0x00;
}

bool matchesActionCodes(const Bytes& tvr, const Bytes& terminalCode, const Bytes& issuerCode) {
    for (std::size_t i = 0; i < tvr::Length; ++i) {
        if (byteAt(tvr, i) & (byteAt(terminalCode, i) | byteAt(issuerCode, i))) return true;
    }
    return false;
}

bool isRecognisedCvm(std::uint8_t cvmCode) {
    switch (cvmCode) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x1E: case 0x1F: return true;
        default: return false;
    }
}

// Case 3 APDU: CLA INS P1 P2 Lc data, with Lc equal to the data length.
bool isWellFormedCommand(const Bytes& command) {
    return command.size() >= 5 && static_cast<std::size_t>(command[4]) == command.size() - 5;
}

} // namespace

SimulatedEmvEngine::SimulatedEmvEngine(std::shared_ptr<const config::EmvConfiguration> configuration,
                                       CardInterface cardInterface, CardBehaviour cardBehaviour)
    : configuration_(std::move(configuration)),
      interface_(cardInterface),
      cardBehaviour_(cardBehaviour),
      log_(cardInterface == CardInterface::Contact ? "[Contact kernel] " : "[Contactless kernel] ") {
    if (!configuration_) throw std::invalid_argument("EMV configuration is required");
}

const std::vector<config::AidConfig>& SimulatedEmvEngine::supportedAids() const {
    return interface_ == CardInterface::Contact ? configuration_->contactAids : configuration_->contactlessAids;
}

void SimulatedEmvEngine::setTvr(const BitFlag& flag) {
    Bytes value = data_.valueOr(tags::TerminalVerificationResults, Bytes(tvr::Length, 0x00));
    setFlag(value, flag);
    data_.set(tags::TerminalVerificationResults, value);
}

void SimulatedEmvEngine::setTsi(const BitFlag& flag) {
    Bytes value = data_.valueOr(tags::TransactionStatusInformation, Bytes(tsi::Length, 0x00));
    setFlag(value, flag);
    data_.set(tags::TransactionStatusInformation, value);
}

std::uint8_t SimulatedEmvEngine::aip() const {
    return byteAt(data_.valueOr(tags::ApplicationInterchangeProfile, {}), 0);
}

std::string SimulatedEmvEngine::odaName() const {
    switch (odaMethod_) {
        case OdaMethod::Sda: return "SDA";
        case OdaMethod::Dda: return interface_ == CardInterface::Contactless ? "fDDA" : "DDA";
        case OdaMethod::Cda: return "CDA";
        case OdaMethod::None: break;
    }
    return "none";
}

std::vector<ApplicationCandidate> SimulatedEmvEngine::buildCandidateList(const CardProfile& card) {
    card_ = findSimulatedCard(card.token);
    if (!card_) throw std::runtime_error("The simulated kernel cannot read card " + card.token);

    std::vector<ApplicationCandidate> candidates;
    for (const auto& application : card_->applications) {
        const Bytes aid = hexToBytes(application.candidate.aid);
        if (config::findSupportedAid(supportedAids(), aid)) {
            std::cout << log_ << "Card AID " << application.candidate.aid << " (" << application.candidate.label
                      << ") is supported by the terminal\n";
            candidates.push_back(application.candidate);
        } else {
            std::cout << log_ << "Card AID " << application.candidate.aid << " (" << application.candidate.label
                      << ") is NOT in the terminal's AID list\n";
        }
    }
    return candidates;
}

void SimulatedEmvEngine::enforceContactlessLimit() const {
    const Bytes* limit = aidConfig_->tags.find(tags::ContactlessTransactionLimit);
    if (limit && amountMinor_ > bcdValue(*limit)) {
        throw std::runtime_error("Amount " + formatAmount(amountMinor_) + " is over the contactless transaction limit of "
                                 + formatAmount(bcdValue(*limit)) + ": insert or swipe the card");
    }
}

std::string SimulatedEmvEngine::initiateApplicationProcessing(const KernelTransaction& transaction) {
    if (!card_) throw std::logic_error("buildCandidateList must run before application processing");
    amountMinor_ = transaction.amountMinor;
    odaMethod_ = OdaMethod::None;
    cardApplication_ = nullptr;
    for (const auto& application : card_->applications) {
        if (application.candidate.aid == transaction.application.aid) cardApplication_ = &application;
    }
    if (!cardApplication_) throw std::runtime_error("Selected application is not on the card");
    const Bytes cardAid = hexToBytes(transaction.application.aid);
    aidConfig_ = config::findSupportedAid(supportedAids(), cardAid);
    if (!aidConfig_) throw std::runtime_error("Selected application is not supported by the terminal");
    if (interface_ == CardInterface::Contactless) enforceContactlessLimit();

    // Terminal data first, then the AID's own parameters on top.
    data_.clear();
    data_.merge(configuration_->terminal);
    data_.merge(aidConfig_->tags);

    const Clock clock = now();
    data_.set(tags::DedicatedFileName, cardAid);
    data_.set(tags::ApplicationIdentifierCard, cardAid);
    data_.set(tags::AmountAuthorised, toBcd(static_cast<std::uint64_t>(amountMinor_), 6));
    data_.set(tags::AmountOther, Bytes(6, 0x00));
    data_.set(tags::TransactionType, {0x00});  // purchase
    data_.set(tags::TransactionDate, clock.date);
    data_.set(tags::TransactionTime, clock.time);
    data_.set(tags::UnpredictableNumber, unpredictableNumber());
    data_.set(tags::TransactionSequenceCounter, toBcd(nextTransactionSequenceCounter(), 4));
    data_.set(tags::TerminalVerificationResults, Bytes(tvr::Length, 0x00));
    data_.set(tags::TransactionStatusInformation, Bytes(tsi::Length, 0x00));

    // GET PROCESSING OPTIONS: the card returns its AIP and AFL and moves its ATC on.
    const TlvStore& card = cardApplication_->records;
    data_.set(tags::ApplicationInterchangeProfile, card.at(tags::ApplicationInterchangeProfile));
    data_.set(tags::ApplicationFileLocator, card.at(tags::ApplicationFileLocator));
    data_.set(tags::ApplicationTransactionCounter, counterBytes(nextApplicationTransactionCounter(*card_)));
    data_.set(tags::LastOnlineAtc, counterBytes(lastOnlineApplicationTransactionCounter(*card_)));

    std::cout << log_ << "SELECT " << toHex(cardAid) << ", GET PROCESSING OPTIONS: AIP "
              << toHex(data_.at(tags::ApplicationInterchangeProfile)) << ", ATC "
              << toHex(data_.at(tags::ApplicationTransactionCounter)) << "\n";
    return "AID " + toHex(cardAid) + " (" + transaction.application.label + ") · AIP "
           + toHex(data_.at(tags::ApplicationInterchangeProfile)) + " · ATC "
           + toHex(data_.at(tags::ApplicationTransactionCounter));
}

std::string SimulatedEmvEngine::readApplicationData() {
    int read = 0;
    for (const auto& field : cardApplication_->records.fields()) {
        bool skip = false;
        for (const Tag tag : kNotReadFromRecords) skip = skip || tag == field.tag;
        if (skip) continue;
        data_.set(field.tag, field.value);
        ++read;
    }
    for (const Tag mandatory : {tags::ApplicationExpirationDate, tags::Pan, tags::CvmList}) {
        if (!data_.contains(mandatory)) setTvr(tvr::IccDataMissing);
    }

    const Bytes pan = data_.valueOr(tags::Pan, {});
    const Bytes expiry = data_.valueOr(tags::ApplicationExpirationDate, {0, 0, 0});
    std::cout << log_ << "READ RECORD: " << read << " data objects, PAN " << maskedPan(pan) << "\n";
    return "PAN " + maskedPan(pan) + " · expires " + toHex({byteAt(expiry, 1)}) + "/" + toHex({byteAt(expiry, 0)})
           + " · " + std::to_string(read) + " data objects read";
}

std::string SimulatedEmvEngine::checkProcessingRestrictions() {
    std::vector<std::string> findings;
    const Bytes* cardVersion = data_.find(tags::ApplicationVersionCard);
    const Bytes* terminalVersion = data_.find(tags::ApplicationVersionTerminal);
    if (cardVersion && terminalVersion && *cardVersion != *terminalVersion) {
        setTvr(tvr::DifferentApplicationVersions);
        findings.push_back("application versions differ (card " + toHex(*cardVersion) + ", terminal "
                           + toHex(*terminalVersion) + ")");
    }

    const std::int64_t today = bcdValue(data_.at(tags::TransactionDate));
    if (const Bytes* expiry = data_.find(tags::ApplicationExpirationDate); expiry && bcdValue(*expiry) < today) {
        setTvr(tvr::ExpiredApplication);
        findings.push_back("card expired");
    }
    if (const Bytes* effective = data_.find(tags::ApplicationEffectiveDate); effective && bcdValue(*effective) > today) {
        setTvr(tvr::ApplicationNotYetEffective);
        findings.push_back("card not yet effective");
    }

    if (const Bytes* usage = data_.find(tags::ApplicationUsageControl); usage && !usage->empty()) {
        const bool domestic = data_.valueOr(tags::IssuerCountryCode, {}) == data_.valueOr(tags::TerminalCountryCode, {});
        const std::uint8_t goodsOrServices = domestic ? 0x28 : 0x14;  // domestic / international goods + services
        const bool allowed = ((*usage)[0] & goodsOrServices) != 0 && ((*usage)[0] & 0x01) != 0;  // 0x01: non-ATM
        if (!allowed) {
            setTvr(tvr::ServiceNotAllowed);
            findings.push_back("purchase not allowed by Application Usage Control");
        }
    }
    return findings.empty() ? "Application version, dates and usage control passed" : join(findings, "; ");
}

std::string SimulatedEmvEngine::selectOfflineDataAuthentication() {
    const std::uint8_t terminalOda = byteAt(data_.valueOr(tags::TerminalCapabilities, {}), 2);
    if ((aip() & 0x01) && (terminalOda & 0x08)) {
        odaMethod_ = OdaMethod::Cda;
    } else if ((aip() & 0x20) && (terminalOda & 0x40)) {
        odaMethod_ = OdaMethod::Dda;
    } else if ((aip() & 0x40) && (terminalOda & 0x80)) {
        odaMethod_ = OdaMethod::Sda;
        setTvr(tvr::SdaSelected);
    } else {
        odaMethod_ = OdaMethod::None;
        setTvr(tvr::OfflineDataAuthenticationNotPerformed);
        return "No ODA method supported by both card and terminal: not performed";
    }
    setTsi(tsi::OfflineDataAuthenticationPerformed);
    std::cout << log_ << "Offline data authentication: " << odaName() << "\n";
    return odaName() + " selected (card AIP " + toHex(data_.at(tags::ApplicationInterchangeProfile))
           + ", terminal capabilities " + toHex(data_.at(tags::TerminalCapabilities)) + ")";
}

std::string SimulatedEmvEngine::authenticateOfflineData() {
    if (odaMethod_ == OdaMethod::None) return "Skipped: no offline data authentication method";

    const BitFlag& failed = odaMethod_ == OdaMethod::Sda   ? tvr::SdaFailed
                            : odaMethod_ == OdaMethod::Dda ? tvr::DdaFailed
                                                           : tvr::CdaFailed;
    const Bytes* index = data_.find(tags::CaPublicKeyIndexCard);
    if (!index || index->size() != 1) {
        setTvr(tvr::IccDataMissing);
        setTvr(failed);
        return "Card has no CA public key index (8F): " + odaName() + " failed";
    }
    const Bytes aid = data_.at(tags::DedicatedFileName);
    const Bytes rid(aid.begin(), aid.begin() + 5);
    const std::string keyName = toHex(rid) + "/" + toHex(*index);
    const config::CaPublicKey* key = config::findCaPublicKey(*configuration_, rid, index->front());
    if (!key) {
        setTvr(failed);
        std::cout << log_ << "CA public key " << keyName << " not available: " << odaName() << " failed\n";
        return "CA public key " + keyName + " not loaded (missing or rejected at start-up): " + odaName() + " failed";
    }
    std::cout << log_ << "Recovered issuer and ICC public keys with CA key " << keyName << " (simulated)\n";
    return "CA public key " + keyName + " (" + std::to_string(key->modulus.size() * 8)
           + "-bit, checksum verified) recovered the issuer and ICC certificates (simulated)";
}

bool SimulatedEmvEngine::terminalSupportsCvm(std::uint8_t cvmCode) const {
    const std::uint8_t cvm = byteAt(data_.valueOr(tags::TerminalCapabilities, {}), 1);
    switch (cvmCode) {
        case 0x00: return true;
        case 0x01: return (cvm & 0x80) != 0;
        case 0x02: return (cvm & 0x40) != 0;
        case 0x03: return (cvm & 0x80) && (cvm & 0x20);
        case 0x04: return (cvm & 0x10) != 0;
        case 0x05: return (cvm & 0x10) && (cvm & 0x20);
        case 0x1E: return (cvm & 0x20) != 0;
        case 0x1F: return (cvm & 0x08) != 0;
        default: return false;
    }
}

bool SimulatedEmvEngine::cvmConditionApplies(std::uint8_t condition, std::uint8_t cvmCode, std::uint64_t x,
                                             std::uint64_t y) const {
    const auto amount = static_cast<std::uint64_t>(amountMinor_);
    const bool sameCurrency =
        data_.valueOr(tags::ApplicationCurrencyCode, {}) == data_.valueOr(tags::TransactionCurrencyCode, {});
    switch (condition) {
        case 0x00: return true;                          // always
        case 0x02: return true;                          // not unattended cash / manual cash / cashback
        case 0x03: return terminalSupportsCvm(cvmCode);  // if terminal supports the CVM
        case 0x06: return sameCurrency && amount < x;
        case 0x07: return sameCurrency && amount > x;
        case 0x08: return sameCurrency && amount < y;
        case 0x09: return sameCurrency && amount > y;
        default: return false;                           // cash, cashback and unknown conditions
    }
}

std::string SimulatedEmvEngine::performCvm(std::uint8_t rule, std::uint8_t condition, bool& failed) {
    std::uint8_t result = 0x00;
    std::string detail;
    switch (rule & 0x3F) {
        case 0x00: result = 0x01; detail = "Card's CVM list says fail CVM"; break;
        case 0x01: case 0x04: result = 0x02; detail = "Offline PIN verified by the card (simulated PIN pad)"; break;
        case 0x03: case 0x05: detail = "Offline PIN verified (simulated) and signature required"; break;
        case 0x02:
            setTvr(tvr::OnlinePinEntered);
            detail = "Online PIN entered on the simulated PIN pad; the issuer verifies it";
            break;
        case 0x1E: detail = "Signature required on the receipt"; break;
        case 0x1F: result = 0x02; detail = "No CVM required"; break;
        default: break;
    }
    data_.set(tags::CvmResults, {rule, condition, result});
    failed = result == 0x01;
    return detail;
}

std::string SimulatedEmvEngine::verifyCardholderContact() {
    if (!(aip() & 0x10)) {
        data_.set(tags::CvmResults, {0x3F, 0x00, 0x00});
        return "Card does not support cardholder verification";
    }
    setTsi(tsi::CardholderVerificationPerformed);
    const Bytes* list = data_.find(tags::CvmList);
    if (!list || list->size() < 10 || (list->size() - 8) % 2 != 0) {
        setTvr(tvr::IccDataMissing);
        setTvr(tvr::CardholderVerificationFailed);
        data_.set(tags::CvmResults, {0x3F, 0x00, 0x01});
        return "CVM List missing or malformed: cardholder verification failed";
    }

    const std::uint64_t x = binaryValue(Bytes(list->begin(), list->begin() + 4));
    const std::uint64_t y = binaryValue(Bytes(list->begin() + 4, list->begin() + 8));
    for (std::size_t i = 8; i + 1 < list->size(); i += 2) {
        const std::uint8_t rule = (*list)[i];
        const std::uint8_t condition = (*list)[i + 1];
        const std::uint8_t code = rule & 0x3F;
        const bool tryNextOnFailure = (rule & 0x40) != 0;
        if (!cvmConditionApplies(condition, code, x, y)) continue;

        if (!isRecognisedCvm(code) || !terminalSupportsCvm(code)) {
            if (!isRecognisedCvm(code)) setTvr(tvr::UnrecognisedCvm);
            if (tryNextOnFailure) continue;
            setTvr(tvr::CardholderVerificationFailed);
            data_.set(tags::CvmResults, {rule, condition, 0x01});
            return describeCvmCode(code) + " not supported by the terminal: cardholder verification failed";
        }
        bool failed = false;
        const std::string detail = performCvm(rule, condition, failed);
        if (failed && tryNextOnFailure) continue;
        if (failed) setTvr(tvr::CardholderVerificationFailed);
        std::cout << log_ << "CVM: " << detail << "\n";
        return detail;
    }
    setTvr(tvr::CardholderVerificationFailed);
    data_.set(tags::CvmResults, {0x3F, 0x00, 0x01});
    return "No CVM rule could be applied: cardholder verification failed";
}

std::string SimulatedEmvEngine::verifyCardholderContactless() {
    setTsi(tsi::CardholderVerificationPerformed);
    const Bytes* limit = aidConfig_->tags.find(tags::ContactlessCvmRequiredLimit);
    if (!limit || amountMinor_ < bcdValue(*limit)) {
        data_.set(tags::CvmResults, {0x1F, 0x00, 0x02});
        return limit ? "Below the contactless CVM limit (" + formatAmount(bcdValue(*limit)) + "): no CVM"
                     : "No contactless CVM limit configured: no CVM";
    }
    const Bytes* ttq = data_.find(tags::TerminalTransactionQualifiers);
    const bool onlinePinSupported = !ttq || (byteAt(*ttq, 0) & 0x04) != 0;
    if (onlinePinSupported) {
        setTvr(tvr::OnlinePinEntered);
        data_.set(tags::CvmResults, {0x02, 0x00, 0x00});
        return "Amount at or over the CVM limit (" + formatAmount(bcdValue(*limit))
               + "): online PIN entered on the simulated PIN pad";
    }
    data_.set(tags::CvmResults, {0x1E, 0x00, 0x00});
    return "Amount at or over the CVM limit (" + formatAmount(bcdValue(*limit)) + "): signature required";
}

std::string SimulatedEmvEngine::verifyCardholder() {
    return interface_ == CardInterface::Contact ? verifyCardholderContact() : verifyCardholderContactless();
}

std::string SimulatedEmvEngine::manageTerminalRisk() {
    if (interface_ == CardInterface::Contact && !(aip() & 0x08)) {
        return "Card does not request terminal risk management";
    }
    setTsi(tsi::TerminalRiskManagementPerformed);
    std::vector<std::string> findings;

    // Floor limit: contact uses 9F1B (binary); contactless uses the reader floor limit (BCD) when present.
    const Bytes* contactlessFloor = aidConfig_->tags.find(tags::ContactlessFloorLimit);
    const bool useContactlessFloor = interface_ == CardInterface::Contactless && contactlessFloor;
    const auto floorLimit = useContactlessFloor
        ? bcdValue(*contactlessFloor)
        : static_cast<std::int64_t>(binaryValue(data_.valueOr(tags::TerminalFloorLimit, {})));
    const bool exceeded = useContactlessFloor ? amountMinor_ > floorLimit : amountMinor_ >= floorLimit;
    if (exceeded) {
        setTvr(tvr::ExceedsFloorLimit);
        findings.push_back("amount over floor limit " + formatAmount(floorLimit));
    } else {
        findings.push_back("amount within floor limit " + formatAmount(floorLimit));
    }

    // Velocity checking against the card's consecutive offline limits.
    const auto atc = binaryValue(data_.at(tags::ApplicationTransactionCounter));
    const auto lastOnline = binaryValue(data_.valueOr(tags::LastOnlineAtc, {}));
    if (lastOnline == 0) {
        setTvr(tvr::NewCard);
        findings.push_back("new card");
    } else if (const Bytes* lower = data_.find(tags::LowerConsecutiveOfflineLimit)) {
        const auto offline = atc - lastOnline;
        if (offline > binaryValue(*lower)) setTvr(tvr::LowerOfflineLimitExceeded);
        if (const Bytes* upper = data_.find(tags::UpperConsecutiveOfflineLimit); upper && offline > binaryValue(*upper)) {
            setTvr(tvr::UpperOfflineLimitExceeded);
        }
        findings.push_back(std::to_string(offline) + " since last online (offline limits " + std::to_string(binaryValue(*lower))
                           + "/" + std::to_string(binaryValue(data_.valueOr(tags::UpperConsecutiveOfflineLimit, {}))) + ")");
    }
    return "Random selection not configured · " + join(findings, " · ");
}

std::string SimulatedEmvEngine::analyseTerminalActions() {
    const Bytes tvr = data_.at(tags::TerminalVerificationResults);
    const Bytes none(tvr::Length, 0x00);
    const Bytes all(tvr::Length, 0xFF);  // issuer action codes default to "all bits" when the card has none
    const Bytes tacDenial = data_.valueOr(tags::TacDenial, none);
    const Bytes tacOnline = data_.valueOr(tags::TacOnline, none);
    const Bytes tacDefault = data_.valueOr(tags::TacDefault, none);
    const Bytes iacDenial = data_.valueOr(tags::IacDenial, none);
    const Bytes iacOnline = data_.valueOr(tags::IacOnline, all);
    const Bytes iacDefault = data_.valueOr(tags::IacDefault, all);

    // Terminal type x1/x4: online only; x2/x5: offline with online capability; x3/x6: offline only.
    const int capability = byteAt(data_.valueOr(tags::TerminalType, {}), 0) & 0x0F;
    const bool onlineCapable = capability != 3 && capability != 6;

    std::string reason;
    if (matchesActionCodes(tvr, tacDenial, iacDenial)) {
        requestedCryptogram_ = CryptogramType::AAC;
        reason = "matches TAC/IAC-Denial";
    } else if (onlineCapable) {
        const bool online = matchesActionCodes(tvr, tacOnline, iacOnline);
        requestedCryptogram_ = online ? CryptogramType::ARQC : CryptogramType::TC;
        reason = online ? "matches TAC/IAC-Online" : "no TAC/IAC-Online match";
    } else {
        const bool decline = matchesActionCodes(tvr, tacDefault, iacDefault);
        requestedCryptogram_ = decline ? CryptogramType::AAC : CryptogramType::TC;
        reason = decline ? "offline-only terminal, matches TAC/IAC-Default" : "offline-only terminal, no default match";
    }
    std::cout << log_ << "Terminal action analysis: TVR " << toHex(tvr) << " " << reason << " -> request "
              << toString(requestedCryptogram_) << "\n";
    return "TVR " + toHex(tvr) + " " + reason + " → request " + toString(requestedCryptogram_);
}

Bytes SimulatedEmvEngine::setCryptogram(CryptogramType type) {
    const std::uint8_t cid = cryptogramInformationData(type);
    data_.set(tags::CryptogramInformationData, {cid});
    const Bytes cryptogram = mockApplicationCryptogram(data_, cid);
    data_.set(tags::ApplicationCryptogram, cryptogram);
    return cryptogram;
}

FirstCryptogram SimulatedEmvEngine::generateFirstAc() {
    // Card risk management: the card may answer with the requested cryptogram or a lower one.
    CryptogramType answer = requestedCryptogram_;
    if (cardBehaviour_ == CardBehaviour::Decline) answer = CryptogramType::AAC;
    if (cardBehaviour_ == CardBehaviour::ForceOnline && answer == CryptogramType::TC) answer = CryptogramType::ARQC;

    const Bytes cryptogram = setCryptogram(answer);
    setTsi(tsi::CardRiskManagementPerformed);
    std::cout << log_ << "GENERATE AC: terminal requested " << toString(requestedCryptogram_) << ", card returned "
              << toString(answer) << " " << toHex(cryptogram) << "\n";

    TlvList onlineData;
    for (const Tag tag : kOnlineDataTags) {
        if (const Bytes* value = data_.find(tag)) onlineData.push_back({tag, *value});
    }
    return {answer, toHex(cryptogram), encodeTlv(onlineData)};
}

std::string SimulatedEmvEngine::authenticateIssuer(const HostResponse& response) {
    if (!response.issuerAuthenticationData) return "No issuer authentication data received";
    const Bytes& data = *response.issuerAuthenticationData;
    data_.set(tags::IssuerAuthenticationData, data);
    if (!(aip() & 0x04)) return "Card does not support issuer authentication";

    setTsi(tsi::IssuerAuthenticationPerformed);
    const Bytes arqc = data_.at(tags::ApplicationCryptogram);
    const bool valid = data.size() >= 10
        && Bytes(data.begin(), data.begin() + 8) == mockArpc(arqc, Bytes(data.begin() + 8, data.begin() + 10));
    if (!valid) {
        setTvr(tvr::IssuerAuthenticationFailed);
        std::cout << log_ << "EXTERNAL AUTHENTICATE: ARPC does not match the ARQC\n";
        return "Issuer authentication failed: ARPC does not match the ARQC";
    }
    std::cout << log_ << "EXTERNAL AUTHENTICATE: ARPC verified (mock)\n";
    return "Issuer authentication passed (ARPC verified, mock)";
}

void SimulatedEmvEngine::runIssuerScripts(const TlvList& scripts, Tag templateTag, Bytes& results,
                                          std::vector<std::string>& notes) {
    const bool beforeFinalAc = templateTag == tags::IssuerScriptTemplate1;
    for (const auto& script : scripts) {
        if (script.tag != templateTag) continue;
        Bytes identifier(4, 0x00);
        int commands = 0;
        int failedAt = 0;
        try {
            for (const auto& item : parseTlv(script.value)) {
                if (item.tag == tags::IssuerScriptIdentifier && item.value.size() == 4) identifier = item.value;
                if (item.tag != tags::IssuerScriptCommand) continue;
                ++commands;
                if (!isWellFormedCommand(item.value)) {
                    if (failedAt == 0) failedAt = commands;
                    continue;
                }
                std::cout << log_ << "Issuer script command to card: " << toHex(item.value) << " -> 9000 (simulated)\n";
            }
        } catch (const std::invalid_argument&) {
            failedAt = commands + 1;
        }

        const bool succeeded = failedAt == 0 && commands > 0;
        results.push_back(succeeded ? std::uint8_t{0x20} : static_cast<std::uint8_t>(0x10 | (failedAt & 0x0F)));
        results.insert(results.end(), identifier.begin(), identifier.end());
        if (!succeeded) setTvr(beforeFinalAc ? tvr::ScriptFailedBeforeFinalAc : tvr::ScriptFailedAfterFinalAc);

        const std::string when = beforeFinalAc ? "before" : "after";
        notes.push_back("Script " + toHex(identifier) + " (template " + tagToHex(templateTag) + "): "
                        + (succeeded ? std::to_string(commands) + " command(s) applied " + when + " the final GENERATE AC"
                                     : "failed at command " + std::to_string(failedAt)));
    }
}

OnlineCompletion SimulatedEmvEngine::completeOnlineTransaction(const HostResponse& response) {
    const Bytes arc = responseCodeToArc(response.responseCode);
    data_.set(tags::AuthorisationResponseCode, arc);
    OnlineCompletion completion;

    if (interface_ == CardInterface::Contactless) {
        if (response.issuerAuthenticationData) data_.set(tags::IssuerAuthenticationData, *response.issuerAuthenticationData);
        completion.issuerAuthenticationStatus = "Issuer authentication not performed: the card has left the field";
        completion.issuerScriptStatus = response.issuerScripts ? "Issuer scripts not delivered: the card has left the field"
                                                               : "No issuer scripts received";
        return completion;
    }

    completion.issuerAuthenticationStatus = authenticateIssuer(response);

    TlvList scripts;
    std::vector<std::string> notes;
    Bytes results;
    if (response.issuerScripts) {
        try {
            scripts = parseTlv(*response.issuerScripts);
        } catch (const std::invalid_argument&) {
            setTvr(tvr::ScriptFailedBeforeFinalAc);
            notes.push_back("Malformed issuer script data ignored");
        }
    }
    runIssuerScripts(scripts, tags::IssuerScriptTemplate1, results, notes);

    // Second GENERATE AC: request TC only if the issuer approved; the card refuses if issuer
    // authentication failed.
    const bool issuerApproved = response.approved && arc == responseCodeToArc("00");
    const bool issuerAuthenticationFailed =
        hasFlag(data_.at(tags::TerminalVerificationResults), tvr::IssuerAuthenticationFailed);
    const CryptogramType answer =
        issuerApproved && !issuerAuthenticationFailed ? CryptogramType::TC : CryptogramType::AAC;
    const Bytes cryptogram = setCryptogram(answer);
    if (answer == CryptogramType::TC) {
        recordOnlineApproval(*card_, static_cast<std::uint16_t>(binaryValue(data_.at(tags::ApplicationTransactionCounter))));
    }
    std::cout << log_ << "Second GENERATE AC: requested " << (issuerApproved ? "TC" : "AAC") << ", card returned "
              << toString(answer) << "\n";

    runIssuerScripts(scripts, tags::IssuerScriptTemplate2, results, notes);
    if (!results.empty()) {
        data_.set(tags::IssuerScriptResults, results);
        setTsi(tsi::ScriptProcessingPerformed);
    }

    completion.finalCryptogram = FinalCryptogram{answer, toHex(cryptogram)};
    completion.issuerScriptsProcessed = !results.empty();
    completion.issuerScriptStatus = notes.empty() ? "No issuer scripts received" : join(notes, "; ");
    return completion;
}

TlvList SimulatedEmvEngine::transactionData() const {
    TlvList data;
    for (const Tag tag : kDisplayTags) {
        if (const Bytes* value = data_.find(tag)) data.push_back({tag, *value});
    }
    return data;
}

} // namespace emv::simulated
