#include "common/kernel_catalog.hpp"
#include "simulated/register_simulated_kernels.hpp"
#include <iostream>
#include <utility>

#ifndef EMV_DEFAULT_CONFIG_DIR
#define EMV_DEFAULT_CONFIG_DIR "config"
#endif

namespace emv::app {

std::filesystem::path defaultConfigDirectory() {
    return EMV_DEFAULT_CONFIG_DIR;
}

LoadedConfiguration loadConfiguration(const std::filesystem::path& directory) {
    config::LoadResult result = config::loadEmvConfiguration(directory);
    std::cout << "EMV configuration " << directory.string() << ": " << result.configuration.contactAids.size()
              << " contact AIDs, " << result.configuration.contactlessAids.size() << " contactless AIDs, "
              << result.configuration.caPublicKeys.size() << " CA public keys\n";
    for (const auto& warning : result.warnings) std::cout << "  WARNING " << warning << "\n";
    return {directory, std::make_shared<const config::EmvConfiguration>(std::move(result.configuration)),
            std::move(result.warnings)};
}

KernelRegistry buildKernelRegistry(std::shared_ptr<const config::EmvConfiguration> configuration,
                                   const simulated::SimulationOptions& simulation) {
    KernelRegistry registry;
    simulated::registerSimulatedKernels(registry, std::move(configuration), simulation);
    // Vendor kernels register here, e.g. vendor_x::registerKernels(registry, configuration);
    return registry;
}

std::string defaultKernelFor(EntryMode mode) {
    return mode == EntryMode::Tap ? simulated::kContactlessKernelName : simulated::kContactKernelName;
}

} // namespace emv::app
