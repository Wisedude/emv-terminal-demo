#include "emv/emv_tags.hpp"
#include <array>
#include <utility>

namespace emv {
namespace {

constexpr std::array<BitFlag, 27> kTvrFlags{
    tvr::OfflineDataAuthenticationNotPerformed, tvr::SdaFailed, tvr::IccDataMissing, tvr::CardOnExceptionFile,
    tvr::DdaFailed, tvr::CdaFailed, tvr::SdaSelected, tvr::DifferentApplicationVersions, tvr::ExpiredApplication,
    tvr::ApplicationNotYetEffective, tvr::ServiceNotAllowed, tvr::NewCard, tvr::CardholderVerificationFailed,
    tvr::UnrecognisedCvm, tvr::PinTryLimitExceeded, tvr::PinPadNotPresent, tvr::PinNotEntered,
    tvr::OnlinePinEntered, tvr::ExceedsFloorLimit, tvr::LowerOfflineLimitExceeded, tvr::UpperOfflineLimitExceeded,
    tvr::RandomlySelectedOnline, tvr::MerchantForcedOnline, tvr::DefaultTdolUsed, tvr::IssuerAuthenticationFailed,
    tvr::ScriptFailedBeforeFinalAc, tvr::ScriptFailedAfterFinalAc};

constexpr std::array<BitFlag, 6> kTsiFlags{
    tsi::OfflineDataAuthenticationPerformed, tsi::CardholderVerificationPerformed, tsi::CardRiskManagementPerformed,
    tsi::IssuerAuthenticationPerformed, tsi::TerminalRiskManagementPerformed, tsi::ScriptProcessingPerformed};

const std::pair<Tag, const char*> kTagNames[] = {
    {tags::ApplicationIdentifierCard, "Application Identifier (card)"},
    {tags::ApplicationLabel, "Application Label"},
    {tags::Track2EquivalentData, "Track 2 Equivalent Data"},
    {tags::Pan, "Application PAN"},
    {tags::IssuerScriptTemplate1, "Issuer Script Template 1"},
    {tags::IssuerScriptTemplate2, "Issuer Script Template 2"},
    {tags::ApplicationInterchangeProfile, "Application Interchange Profile"},
    {tags::DedicatedFileName, "Dedicated File (DF) Name"},
    {tags::IssuerScriptCommand, "Issuer Script Command"},
    {tags::ApplicationPriorityIndicator, "Application Priority Indicator"},
    {tags::AuthorisationResponseCode, "Authorisation Response Code"},
    {tags::CvmList, "CVM List"},
    {tags::CaPublicKeyIndexCard, "CA Public Key Index (card)"},
    {tags::IssuerAuthenticationData, "Issuer Authentication Data"},
    {tags::ApplicationFileLocator, "Application File Locator"},
    {tags::TerminalVerificationResults, "Terminal Verification Results"},
    {tags::TransactionDate, "Transaction Date"},
    {tags::TransactionStatusInformation, "Transaction Status Information"},
    {tags::TransactionType, "Transaction Type"},
    {tags::ApplicationExpirationDate, "Application Expiration Date"},
    {tags::ApplicationEffectiveDate, "Application Effective Date"},
    {tags::IssuerCountryCode, "Issuer Country Code"},
    {tags::TransactionCurrencyCode, "Transaction Currency Code"},
    {tags::PanSequenceNumber, "PAN Sequence Number"},
    {tags::TransactionCurrencyExponent, "Transaction Currency Exponent"},
    {tags::AmountAuthorised, "Amount, Authorised"},
    {tags::AmountOther, "Amount, Other"},
    {tags::AidTerminal, "Application Identifier (terminal)"},
    {tags::ApplicationUsageControl, "Application Usage Control"},
    {tags::ApplicationVersionCard, "Application Version Number (card)"},
    {tags::ApplicationVersionTerminal, "Application Version Number (terminal)"},
    {tags::IacDefault, "Issuer Action Code - Default"},
    {tags::IacDenial, "Issuer Action Code - Denial"},
    {tags::IacOnline, "Issuer Action Code - Online"},
    {tags::IssuerApplicationData, "Issuer Application Data"},
    {tags::LastOnlineAtc, "Last Online ATC Register"},
    {tags::LowerConsecutiveOfflineLimit, "Lower Consecutive Offline Limit"},
    {tags::IssuerScriptIdentifier, "Issuer Script Identifier"},
    {tags::TerminalCountryCode, "Terminal Country Code"},
    {tags::TerminalFloorLimit, "Terminal Floor Limit"},
    {tags::InterfaceDeviceSerialNumber, "Interface Device Serial Number"},
    {tags::TransactionTime, "Transaction Time"},
    {tags::CaPublicKeyIndex, "CA Public Key Index"},
    {tags::UpperConsecutiveOfflineLimit, "Upper Consecutive Offline Limit"},
    {tags::ApplicationCryptogram, "Application Cryptogram"},
    {tags::CryptogramInformationData, "Cryptogram Information Data"},
    {tags::TerminalCapabilities, "Terminal Capabilities"},
    {tags::CvmResults, "CVM Results"},
    {tags::TerminalType, "Terminal Type"},
    {tags::ApplicationTransactionCounter, "Application Transaction Counter"},
    {tags::UnpredictableNumber, "Unpredictable Number"},
    {tags::AdditionalTerminalCapabilities, "Additional Terminal Capabilities"},
    {tags::TransactionSequenceCounter, "Transaction Sequence Counter"},
    {tags::ApplicationCurrencyCode, "Application Currency Code"},
    {tags::IssuerScriptResults, "Issuer Script Results"},
    {tags::TerminalTransactionQualifiers, "Terminal Transaction Qualifiers"},
    {tags::TacDefault, "Terminal Action Code - Default"},
    {tags::TacDenial, "Terminal Action Code - Denial"},
    {tags::TacOnline, "Terminal Action Code - Online"},
    {tags::ContactlessTransactionLimit, "Contactless Transaction Limit"},
    {tags::ContactlessCvmRequiredLimit, "Contactless CVM Required Limit"},
    {tags::ContactlessFloorLimit, "Contactless Floor Limit"},
};

template <std::size_t N>
std::vector<std::string> describe(const Bytes& field, const std::array<BitFlag, N>& flags) {
    std::vector<std::string> set;
    for (const auto& flag : flags) {
        if (hasFlag(field, flag)) set.emplace_back(flag.description);
    }
    return set;
}

std::string describeCvmCondition(std::uint8_t condition) {
    switch (condition) {
        case 0x00: return "always";
        case 0x01: return "if unattended cash";
        case 0x02: return "if not unattended cash, manual cash or cashback";
        case 0x03: return "if terminal supports the CVM";
        case 0x04: return "if manual cash";
        case 0x05: return "if purchase with cashback";
        case 0x06: return "if amount under X";
        case 0x07: return "if amount over X";
        case 0x08: return "if amount under Y";
        case 0x09: return "if amount over Y";
        default: return "condition " + toHex({condition});
    }
}

} // namespace

std::string tagName(Tag tag) {
    for (const auto& [known, name] : kTagNames) {
        if (known == tag) return name;
    }
    return "Proprietary / unknown";
}

void setFlag(Bytes& field, const BitFlag& flag) {
    if (field.size() <= flag.byteIndex) field.resize(flag.byteIndex + 1, 0x00);
    field[flag.byteIndex] = static_cast<std::uint8_t>(field[flag.byteIndex] | flag.mask);
}

bool hasFlag(const Bytes& field, const BitFlag& flag) {
    return field.size() > flag.byteIndex && (field[flag.byteIndex] & flag.mask) != 0;
}

std::vector<std::string> describeTvr(const Bytes& tvr) {
    return describe(tvr, kTvrFlags);
}

std::vector<std::string> describeTsi(const Bytes& tsi) {
    return describe(tsi, kTsiFlags);
}

std::string describeCvmCode(std::uint8_t cvmCode) {
    switch (cvmCode & 0x3F) {
        case 0x00: return "Fail CVM processing";
        case 0x01: return "Plaintext PIN verified by ICC";
        case 0x02: return "Enciphered PIN verified online";
        case 0x03: return "Plaintext PIN verified by ICC and signature";
        case 0x04: return "Enciphered PIN verified by ICC";
        case 0x05: return "Enciphered PIN verified by ICC and signature";
        case 0x1E: return "Signature (paper)";
        case 0x1F: return "No CVM required";
        case 0x3F: return "No CVM performed";
        default: return "Unrecognised CVM " + toHex({static_cast<std::uint8_t>(cvmCode & 0x3F)});
    }
}

std::string describeCvmResults(const Bytes& cvmResults) {
    if (cvmResults.size() != 3) return "Invalid CVM Results";
    std::string result;
    switch (cvmResults[2]) {
        case 0x00: result = "result unknown"; break;
        case 0x01: result = "failed"; break;
        case 0x02: result = "successful"; break;
        default: result = "result " + toHex({cvmResults[2]}); break;
    }
    return describeCvmCode(cvmResults[0]) + " · " + describeCvmCondition(cvmResults[1]) + " · " + result;
}

} // namespace emv
