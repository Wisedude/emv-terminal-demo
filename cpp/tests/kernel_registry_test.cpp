#include "emv/kernel/kernel_registry.hpp"
#include "emv/policy/preferred_aid_selector.hpp"
#include "test_support.hpp"
#include <memory>

namespace {

using namespace emv;
using namespace emv::test;

void createsRegisteredKernelsByName() {
    KernelRegistry registry;
    registry.add("approve-offline", [] { return std::make_unique<FakeKernel>(CryptogramType::TC); });
    registry.add("decline-offline", [] { return std::make_unique<FakeKernel>(CryptogramType::AAC); });

    const auto kernel = registry.create("decline-offline");
    NullTransactionObserver observer;
    require(kernel != nullptr, "registered kernel must be created");
    require(kernel->processUntilFirstCryptogram({}, observer).type == CryptogramType::AAC, "the named kernel must be created");
    require(registry.contains("approve-offline"), "registry must report registered names");
    require((registry.names() == std::vector<std::string>{"approve-offline", "decline-offline"}),
            "names must list every registered kernel");
}

void rejectsUnknownDuplicateAndInvalidRegistrations() {
    KernelRegistry registry;
    registry.add("k", [] { return std::make_unique<FakeKernel>(CryptogramType::TC); });

    requireThrows<std::out_of_range>([&] { registry.create("missing"); }, "unknown kernel must throw");
    requireThrows<std::invalid_argument>(
        [&] { registry.add("k", [] { return std::make_unique<FakeKernel>(CryptogramType::TC); }); },
        "duplicate name must throw");
    requireThrows<std::invalid_argument>([&] { registry.add("", [] { return std::make_unique<FakeKernel>(CryptogramType::TC); }); },
                                         "empty name must throw");
    requireThrows<std::invalid_argument>([&] { registry.add("empty", KernelRegistry::Factory{}); },
                                         "empty factory must throw");
}

void preferredAidSelectorHonoursPreferenceOrder() {
    const std::vector<ApplicationCandidate> candidates{{"A0000000041010", "MC"}, {"A0000000031010", "Visa"}};

    PreferredAidSelector preferVisa({"A0000000031010"});
    require(preferVisa.select(candidates)->aid == "A0000000031010", "preferred AID must win");

    PreferredAidSelector noMatch({"A0000000999999"});
    require(noMatch.select(candidates)->aid == "A0000000041010", "falls back to the kernel's first candidate");
    require(!noMatch.select({}).has_value(), "no candidates means no selection");
}

} // namespace

int main() {
    return runTests("KernelRegistry", {
        {"createsRegisteredKernelsByName", createsRegisteredKernelsByName},
        {"rejectsUnknownDuplicateAndInvalidRegistrations", rejectsUnknownDuplicateAndInvalidRegistrations},
        {"preferredAidSelectorHonoursPreferenceOrder", preferredAidSelectorHonoursPreferenceOrder},
    });
}
