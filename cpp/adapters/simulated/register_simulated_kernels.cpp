#include "simulated/register_simulated_kernels.hpp"
#include "simulated/simulated_contact_kernel.hpp"
#include "simulated/simulated_contactless_kernel.hpp"
#include <memory>

namespace emv::simulated {

void registerSimulatedKernels(KernelRegistry& registry, std::shared_ptr<const config::EmvConfiguration> configuration,
                              SimulationOptions options) {
    registry.add(kContactKernelName, [configuration, options] {
        return std::make_unique<SimulatedContactKernel>(configuration, options);
    });
    registry.add(kContactlessKernelName, [configuration, options] {
        return std::make_unique<SimulatedContactlessKernel>(configuration, options);
    });
}

} // namespace emv::simulated
