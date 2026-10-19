#pragma once
#include "emv/kernel/kernel_registry.hpp"

namespace emv::simulated {

inline constexpr const char* kContactKernelName = "simulated-contact";
inline constexpr const char* kContactlessKernelName = "simulated-contactless";

// Adds this library's kernels to the registry. Each vendor adapter library provides an equivalent.
void registerSimulatedKernels(KernelRegistry& registry);

} // namespace emv::simulated
