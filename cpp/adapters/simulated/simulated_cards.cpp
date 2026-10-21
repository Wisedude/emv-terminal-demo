#include "simulated/simulated_cards.hpp"
#include "emv/emv_tags.hpp"
#include <map>
#include <mutex>
#include <stdexcept>

namespace emv::simulated {
namespace {

struct CardProfileData {
    const char* aid;
    const char* label;
    const char* pan;  // public test-card ranges only
    const char* expiry;
    const char* aip;
    const char* cvmList;
    const char* iacDefault;
    const char* iacDenial;
    const char* iacOnline;
    const char* applicationVersion;
    const char* caPublicKeyIndex;
    const char* issuerApplicationData;
};

Bytes ascii(const char* text) {
    const std::string value(text);
    return Bytes(value.begin(), value.end());
}

SimulatedCardApplication makeApplication(const CardProfileData& data) {
    TlvStore records;
    records.set(tags::ApplicationInterchangeProfile, hexToBytes(data.aip));
    records.set(tags::ApplicationFileLocator, hexToBytes("08 01 01 00 10 01 03 01 18 01 02 00"));
    records.set(tags::ApplicationLabel, ascii(data.label));
    records.set(tags::ApplicationPriorityIndicator, {0x01});
    records.set(tags::Pan, hexToBytes(data.pan));
    records.set(tags::ApplicationExpirationDate, hexToBytes(data.expiry));
    records.set(tags::ApplicationEffectiveDate, hexToBytes("20 01 01"));
    records.set(tags::IssuerCountryCode, hexToBytes("07 10"));
    records.set(tags::PanSequenceNumber, {0x01});
    records.set(tags::ApplicationUsageControl, hexToBytes("FF 00"));
    records.set(tags::CvmList, hexToBytes(data.cvmList));
    records.set(tags::IacDefault, hexToBytes(data.iacDefault));
    records.set(tags::IacDenial, hexToBytes(data.iacDenial));
    records.set(tags::IacOnline, hexToBytes(data.iacOnline));
    records.set(tags::ApplicationVersionCard, hexToBytes(data.applicationVersion));
    records.set(tags::CaPublicKeyIndexCard, hexToBytes(data.caPublicKeyIndex));
    records.set(tags::ApplicationCurrencyCode, hexToBytes("07 10"));
    records.set(tags::LowerConsecutiveOfflineLimit, {0x0A});
    records.set(tags::UpperConsecutiveOfflineLimit, {0x14});
    records.set(tags::IssuerApplicationData, hexToBytes(data.issuerApplicationData));
    return {{data.aid, data.label}, records};
}

// CVM lists: X and Y amounts (4 bytes each), then 2-byte rules. 0x40 in a rule = "try the next rule if
// this one fails". Visa: online PIN, else signature, else no CVM. Mastercard: signature first.
constexpr CardProfileData kVisa{
    "A0000000031010", "VISA CREDIT", "47 61 73 90 01 01 00 10", "30 12 31", "3C 00",
    "00 00 00 00 00 00 00 00 42 03 5E 03 1F 03", "F0 40 00 88 00", "00 10 00 00 00", "F0 40 04 98 00",
    "00 8C", "92", "06 01 0A 03 A0 00 00"};
constexpr CardProfileData kVisaExpired{
    "A0000000031010", "VISA CREDIT", "47 61 73 90 01 01 01 19", "24 03 31", "3C 00",
    "00 00 00 00 00 00 00 00 42 03 5E 03 1F 03", "F0 40 00 88 00", "00 10 00 00 00", "F0 40 04 98 00",
    "00 8C", "92", "06 01 0A 03 A0 00 00"};
constexpr CardProfileData kMastercard{
    "A0000000041010", "MASTERCARD", "54 13 33 00 89 02 00 11", "29 06 30", "3D 00",
    "00 00 00 00 00 00 00 00 5E 03 42 03 1F 03", "B0 50 84 80 00", "00 00 00 00 00", "B0 70 84 80 00",
    "00 02", "F1", "01 10 A0 40 03 22 00 00 00 00 00 00 00 00 00 00 00 FF"};
// Points at CA key F5, which CAKEYS.TXT ships with a bad checksum, so CDA cannot be performed.
constexpr CardProfileData kMastercardUnknownCaKey{
    "A0000000041010", "MASTERCARD", "54 13 33 00 89 02 00 29", "29 06 30", "3D 00",
    "00 00 00 00 00 00 00 00 5E 03 42 03 1F 03", "B0 50 84 80 00", "00 00 00 00 00", "B0 70 84 80 00",
    "00 02", "F5", "01 10 A0 40 03 22 00 00 00 00 00 00 00 00 00 00 00 FF"};
// JCB is in neither AID file: application selection must fail.
constexpr CardProfileData kJcb{
    "A0000000651010", "JCB CREDIT", "35 40 59 99 99 99 10 47", "30 12 31", "3C 00",
    "00 00 00 00 00 00 00 00 42 03 1F 03", "F0 40 00 88 00", "00 10 00 00 00", "F0 40 04 98 00",
    "02 00", "0F", "06 01 0A 03 A0 00 00"};

const std::vector<SimulatedCard>& cards() {
    static const std::vector<SimulatedCard> all{
        {SimulatedCardId::Visa, "SIMULATED-VISA-01", "Visa credit (test)", {makeApplication(kVisa)}},
        {SimulatedCardId::Mastercard, "SIMULATED-MC-01", "Mastercard credit (test)", {makeApplication(kMastercard)}},
        {SimulatedCardId::VisaExpired, "SIMULATED-VISA-02", "Visa credit, expired (test)",
         {makeApplication(kVisaExpired)}},
        {SimulatedCardId::MastercardUnknownCaKey, "SIMULATED-MC-02", "Mastercard, CA key F5 (test)",
         {makeApplication(kMastercardUnknownCaKey)}},
        {SimulatedCardId::JcbUnsupported, "SIMULATED-JCB-01", "JCB credit, AID not supported (test)",
         {makeApplication(kJcb)}},
    };
    return all;
}

struct CardCounters {
    std::uint16_t atc;
    std::uint16_t lastOnlineAtc;
};

std::mutex& countersMutex() {
    static std::mutex mutex;
    return mutex;
}

CardCounters& counters(const SimulatedCard& card) {
    static std::map<std::string, CardCounters> byToken;
    return byToken.try_emplace(card.token, CardCounters{0x0014, 0x0014}).first->second;
}

} // namespace

const SimulatedCard& simulatedCard(SimulatedCardId id) {
    for (const auto& card : cards()) {
        if (card.id == id) return card;
    }
    throw std::logic_error("Unknown simulated card");
}

const SimulatedCard* findSimulatedCard(const std::string& token) {
    for (const auto& card : cards()) {
        if (card.token == token) return &card;
    }
    return nullptr;
}

std::uint16_t nextApplicationTransactionCounter(const SimulatedCard& card) {
    std::lock_guard<std::mutex> lock(countersMutex());
    return ++counters(card).atc;
}

std::uint16_t lastOnlineApplicationTransactionCounter(const SimulatedCard& card) {
    std::lock_guard<std::mutex> lock(countersMutex());
    return counters(card).lastOnlineAtc;
}

void recordOnlineApproval(const SimulatedCard& card, std::uint16_t atc) {
    std::lock_guard<std::mutex> lock(countersMutex());
    counters(card).lastOnlineAtc = atc;
}

} // namespace emv::simulated
