#pragma once
#include "emv/domain.hpp"
#include "emv/tlv.hpp"
#include "simulated/simulation_options.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace emv::simulated {

// One application on a simulated chip. records holds what a kernel would read from the card: the GET
// PROCESSING OPTIONS response (AIP, AFL) and the READ RECORD data (PAN, dates, CVM list, IACs, ...).
struct SimulatedCardApplication {
    ApplicationCandidate candidate;
    TlvStore records;
};

struct SimulatedCard {
    SimulatedCardId id;
    std::string token;
    std::string description;
    std::vector<SimulatedCardApplication> applications;
};

const SimulatedCard& simulatedCard(SimulatedCardId id);
const SimulatedCard* findSimulatedCard(const std::string& token);

// The card's counters live on the chip: the ATC goes up on every transaction, and the last online ATC
// register is updated when the issuer approves online. Thread-safe.
std::uint16_t nextApplicationTransactionCounter(const SimulatedCard& card);
std::uint16_t lastOnlineApplicationTransactionCounter(const SimulatedCard& card);
void recordOnlineApproval(const SimulatedCard& card, std::uint16_t atc);

} // namespace emv::simulated
