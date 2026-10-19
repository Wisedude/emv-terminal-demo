#include "simulated/register_simulated_kernels.hpp"
#include "simulated/simulated_contact_kernel.hpp"
#include "simulated/simulated_contactless_kernel.hpp"

namespace emv::simulated {

void registerSimulatedKernels(KernelRegistry& registry) {
    registry.add(kContactKernelName, [] { return std::make_unique<SimulatedContactKernel>(); });
    registry.add(kContactlessKernelName, [] { return std::make_unique<SimulatedContactlessKernel>(); });
}

} // namespace emv::simulated
