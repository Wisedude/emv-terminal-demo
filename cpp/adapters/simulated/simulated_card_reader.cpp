#include "simulated/simulated_card_reader.hpp"
#include "simulated/simulated_cards.hpp"
#include <iostream>

namespace emv::simulated {

SimulatedCardReader::SimulatedCardReader(SimulatedCardId card) : card_(card) {}

CardProfile SimulatedCardReader::waitForCard(EntryMode mode) {
    const SimulatedCard& card = simulatedCard(card_);
    std::cout << "[Reader] " << toString(mode) << ": " << card.description << " presented (simulated)\n";

    CardProfile profile{card.token, {}};
    for (const auto& application : card.applications) profile.applications.push_back(application.candidate);
    return profile;
}

} // namespace emv::simulated
