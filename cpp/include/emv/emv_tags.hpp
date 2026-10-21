#pragma once
#include "emv/tlv.hpp"
#include <cstddef>
#include <string>
#include <vector>

// EMV data element tags and the bit layouts of TVR, TSI and CVM Results (EMV Book 3, Annex A and C).
namespace emv {

namespace tags {
inline constexpr Tag ApplicationIdentifierCard = 0x4F;
inline constexpr Tag ApplicationLabel = 0x50;
inline constexpr Tag Track2EquivalentData = 0x57;
inline constexpr Tag Pan = 0x5A;
inline constexpr Tag IssuerScriptTemplate1 = 0x71;
inline constexpr Tag IssuerScriptTemplate2 = 0x72;
inline constexpr Tag ApplicationInterchangeProfile = 0x82;
inline constexpr Tag DedicatedFileName = 0x84;
inline constexpr Tag IssuerScriptCommand = 0x86;
inline constexpr Tag ApplicationPriorityIndicator = 0x87;
inline constexpr Tag AuthorisationResponseCode = 0x8A;
inline constexpr Tag CvmList = 0x8E;
inline constexpr Tag CaPublicKeyIndexCard = 0x8F;
inline constexpr Tag IssuerAuthenticationData = 0x91;
inline constexpr Tag ApplicationFileLocator = 0x94;
inline constexpr Tag TerminalVerificationResults = 0x95;
inline constexpr Tag TransactionDate = 0x9A;
inline constexpr Tag TransactionStatusInformation = 0x9B;
inline constexpr Tag TransactionType = 0x9C;
inline constexpr Tag ApplicationExpirationDate = 0x5F24;
inline constexpr Tag ApplicationEffectiveDate = 0x5F25;
inline constexpr Tag IssuerCountryCode = 0x5F28;
inline constexpr Tag TransactionCurrencyCode = 0x5F2A;
inline constexpr Tag PanSequenceNumber = 0x5F34;
inline constexpr Tag TransactionCurrencyExponent = 0x5F36;
inline constexpr Tag AmountAuthorised = 0x9F02;
inline constexpr Tag AmountOther = 0x9F03;
inline constexpr Tag AidTerminal = 0x9F06;
inline constexpr Tag ApplicationUsageControl = 0x9F07;
inline constexpr Tag ApplicationVersionCard = 0x9F08;
inline constexpr Tag ApplicationVersionTerminal = 0x9F09;
inline constexpr Tag IacDefault = 0x9F0D;
inline constexpr Tag IacDenial = 0x9F0E;
inline constexpr Tag IacOnline = 0x9F0F;
inline constexpr Tag IssuerApplicationData = 0x9F10;
inline constexpr Tag LastOnlineAtc = 0x9F13;
inline constexpr Tag LowerConsecutiveOfflineLimit = 0x9F14;
inline constexpr Tag IssuerScriptIdentifier = 0x9F18;
inline constexpr Tag TerminalCountryCode = 0x9F1A;
inline constexpr Tag TerminalFloorLimit = 0x9F1B;
inline constexpr Tag InterfaceDeviceSerialNumber = 0x9F1E;
inline constexpr Tag TransactionTime = 0x9F21;
inline constexpr Tag CaPublicKeyIndex = 0x9F22;
inline constexpr Tag UpperConsecutiveOfflineLimit = 0x9F23;
inline constexpr Tag ApplicationCryptogram = 0x9F26;
inline constexpr Tag CryptogramInformationData = 0x9F27;
inline constexpr Tag TerminalCapabilities = 0x9F33;
inline constexpr Tag CvmResults = 0x9F34;
inline constexpr Tag TerminalType = 0x9F35;
inline constexpr Tag ApplicationTransactionCounter = 0x9F36;
inline constexpr Tag UnpredictableNumber = 0x9F37;
inline constexpr Tag AdditionalTerminalCapabilities = 0x9F40;
inline constexpr Tag TransactionSequenceCounter = 0x9F41;
inline constexpr Tag ApplicationCurrencyCode = 0x9F42;
inline constexpr Tag IssuerScriptResults = 0x9F5B;
inline constexpr Tag TerminalTransactionQualifiers = 0x9F66;

// Proprietary tags used by the terminal parameter files (not exchanged with the card).
inline constexpr Tag CaPublicKeyChecksum = 0x9F918702;
inline constexpr Tag CaPublicKeyExponent = 0x9F918703;
inline constexpr Tag CaPublicKeyModulus = 0x9F918704;
inline constexpr Tag TacDefault = 0x9F918709;
inline constexpr Tag TacDenial = 0x9F91870A;
inline constexpr Tag TacOnline = 0x9F91870B;
inline constexpr Tag ContactlessTransactionLimit = 0x9F92810D;
inline constexpr Tag ContactlessCvmRequiredLimit = 0x9F92810E;
inline constexpr Tag ContactlessFloorLimit = 0x9F92810F;
} // namespace tags

// Human-readable name of a tag, or "Proprietary / unknown".
std::string tagName(Tag tag);

// One bit of a multi-byte bit-field such as TVR or TSI.
struct BitFlag {
    std::size_t byteIndex;
    std::uint8_t mask;
    const char* description;
};

namespace tvr {
inline constexpr std::size_t Length = 5;
inline constexpr BitFlag OfflineDataAuthenticationNotPerformed{0, 0x80, "Offline data authentication was not performed"};
inline constexpr BitFlag SdaFailed{0, 0x40, "SDA failed"};
inline constexpr BitFlag IccDataMissing{0, 0x20, "ICC data missing"};
inline constexpr BitFlag CardOnExceptionFile{0, 0x10, "Card appears on terminal exception file"};
inline constexpr BitFlag DdaFailed{0, 0x08, "DDA failed"};
inline constexpr BitFlag CdaFailed{0, 0x04, "CDA failed"};
inline constexpr BitFlag SdaSelected{0, 0x02, "SDA selected"};
inline constexpr BitFlag DifferentApplicationVersions{1, 0x80, "ICC and terminal have different application versions"};
inline constexpr BitFlag ExpiredApplication{1, 0x40, "Expired application"};
inline constexpr BitFlag ApplicationNotYetEffective{1, 0x20, "Application not yet effective"};
inline constexpr BitFlag ServiceNotAllowed{1, 0x10, "Requested service not allowed for card product"};
inline constexpr BitFlag NewCard{1, 0x08, "New card"};
inline constexpr BitFlag CardholderVerificationFailed{2, 0x80, "Cardholder verification was not successful"};
inline constexpr BitFlag UnrecognisedCvm{2, 0x40, "Unrecognised CVM"};
inline constexpr BitFlag PinTryLimitExceeded{2, 0x20, "PIN Try Limit exceeded"};
inline constexpr BitFlag PinPadNotPresent{2, 0x10, "PIN entry required and PIN pad not present or not working"};
inline constexpr BitFlag PinNotEntered{2, 0x08, "PIN entry required, PIN pad present, but PIN was not entered"};
inline constexpr BitFlag OnlinePinEntered{2, 0x04, "Online PIN entered"};
inline constexpr BitFlag ExceedsFloorLimit{3, 0x80, "Transaction exceeds floor limit"};
inline constexpr BitFlag LowerOfflineLimitExceeded{3, 0x40, "Lower consecutive offline limit exceeded"};
inline constexpr BitFlag UpperOfflineLimitExceeded{3, 0x20, "Upper consecutive offline limit exceeded"};
inline constexpr BitFlag RandomlySelectedOnline{3, 0x10, "Transaction selected randomly for online processing"};
inline constexpr BitFlag MerchantForcedOnline{3, 0x08, "Merchant forced transaction online"};
inline constexpr BitFlag DefaultTdolUsed{4, 0x80, "Default TDOL used"};
inline constexpr BitFlag IssuerAuthenticationFailed{4, 0x40, "Issuer authentication failed"};
inline constexpr BitFlag ScriptFailedBeforeFinalAc{4, 0x20, "Script processing failed before final GENERATE AC"};
inline constexpr BitFlag ScriptFailedAfterFinalAc{4, 0x10, "Script processing failed after final GENERATE AC"};
} // namespace tvr

namespace tsi {
inline constexpr std::size_t Length = 2;
inline constexpr BitFlag OfflineDataAuthenticationPerformed{0, 0x80, "Offline data authentication was performed"};
inline constexpr BitFlag CardholderVerificationPerformed{0, 0x40, "Cardholder verification was performed"};
inline constexpr BitFlag CardRiskManagementPerformed{0, 0x20, "Card risk management was performed"};
inline constexpr BitFlag IssuerAuthenticationPerformed{0, 0x10, "Issuer authentication was performed"};
inline constexpr BitFlag TerminalRiskManagementPerformed{0, 0x08, "Terminal risk management was performed"};
inline constexpr BitFlag ScriptProcessingPerformed{0, 0x04, "Script processing was performed"};
} // namespace tsi

// Sets or tests a flag; the field is grown with zero bytes if it is too short.
void setFlag(Bytes& field, const BitFlag& flag);
bool hasFlag(const Bytes& field, const BitFlag& flag);

// Descriptions of every set bit, in bit order.
std::vector<std::string> describeTvr(const Bytes& tvr);
std::vector<std::string> describeTsi(const Bytes& tsi);
// e.g. "Enciphered PIN verified online · if terminal supports the CVM · result unknown"
std::string describeCvmResults(const Bytes& cvmResults);
std::string describeCvmCode(std::uint8_t cvmCode);

} // namespace emv
