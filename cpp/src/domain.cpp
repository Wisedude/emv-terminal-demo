#include "emv/domain.hpp"
#include <stdexcept>

namespace emv {

std::string toString(EntryMode mode) {
    switch (mode) {
        case EntryMode::Tap: return "Tap";
        case EntryMode::Insert: return "Insert";
        case EntryMode::Swipe: return "Swipe";
        case EntryMode::Manual: return "Manual entry";
    }
    throw std::logic_error("Unknown entry mode");
}

std::string toString(CryptogramType type) {
    switch (type) {
        case CryptogramType::AAC: return "AAC";
        case CryptogramType::TC: return "TC";
        case CryptogramType::ARQC: return "ARQC";
    }
    throw std::logic_error("Unknown cryptogram type");
}

} // namespace emv
