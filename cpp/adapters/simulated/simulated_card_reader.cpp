#include "simulated/simulated_card_reader.hpp"
#include <iostream>

namespace emv::simulated {

CardProfile SimulatedCardReader::waitForCard(EntryMode mode) {
    std::cout << "[Reader] Waiting for " << toString(mode) << " card (simulated)...\n";
    return {"SIMULATED-CARD-01", {
        {"A0000000031010", "Visa credit (test)"},
        {"A0000000041010", "Mastercard (test)"},
    }};
}

} // namespace emv::simulated
