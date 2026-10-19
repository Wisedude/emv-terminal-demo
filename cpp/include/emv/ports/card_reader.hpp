#pragma once
#include "emv/domain.hpp"

namespace emv {

// Hardware abstraction. A real implementation delegates to the terminal vendor's secure reader APIs.
class ICardReader {
public:
    virtual ~ICardReader() = default;
    virtual CardProfile waitForCard(EntryMode mode) = 0;
};

} // namespace emv
