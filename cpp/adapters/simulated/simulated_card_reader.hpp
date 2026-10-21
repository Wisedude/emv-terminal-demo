#pragma once
#include "emv/ports/card_reader.hpp"
#include "simulated/simulation_options.hpp"

namespace emv::simulated {

// Presents one of the simulated test cards (simulated_cards.hpp). Never reads real cards.
class SimulatedCardReader final : public ICardReader {
public:
    explicit SimulatedCardReader(SimulatedCardId card = SimulatedCardId::Visa);
    CardProfile waitForCard(EntryMode mode) override;

private:
    SimulatedCardId card_;
};

} // namespace emv::simulated
