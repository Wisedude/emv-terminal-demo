#pragma once
#include "emv/ports/card_reader.hpp"

namespace emv::simulated {

// Returns a fixed test card with Visa and Mastercard applications. Never reads real cards.
class SimulatedCardReader final : public ICardReader {
public:
    CardProfile waitForCard(EntryMode mode) override;
};

} // namespace emv::simulated
