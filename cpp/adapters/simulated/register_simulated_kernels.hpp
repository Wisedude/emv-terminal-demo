#pragma once
#include "emv/config/emv_configuration.hpp"
#include "emv/kernel/kernel_registry.hpp"
#include "simulated/simulation_options.hpp"
#include <memory>

namespace emv::simulated {

inline constexpr const char* kContactKernelName = "simulated-contact";
inline constexpr const char* kContactlessKernelName = "simulated-contactless";

// Adds this library's kernels to the registry, loaded with the given EMV configuration.
// Each vendor adapter library provides an equivalent.
void registerSimulatedKernels(KernelRegistry& registry, std::shared_ptr<const config::EmvConfiguration> configuration,
                              SimulationOptions options = {});

} // namespace emv::simulated
