#pragma once
#include "emv/config/emv_configuration.hpp"
#include "emv/kernel/kernel_registry.hpp"
#include "simulated/simulation_options.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Shared by every composition root: where the EMV parameter files live and which kernels this build offers.
namespace emv::app {

struct LoadedConfiguration {
    std::filesystem::path directory;
    std::shared_ptr<const config::EmvConfiguration> emv;
    std::vector<std::string> warnings;
};

// The repository's cpp/config folder, fixed at build time.
std::filesystem::path defaultConfigDirectory();

// Loads the parameter files and prints a summary and every rejected record. Throws config::ConfigError.
LoadedConfiguration loadConfiguration(const std::filesystem::path& directory);

KernelRegistry buildKernelRegistry(std::shared_ptr<const config::EmvConfiguration> configuration,
                                   const simulated::SimulationOptions& simulation = {});

// The kernel used when the caller does not name one: contactless for Tap, contact otherwise.
std::string defaultKernelFor(EntryMode mode);

} // namespace emv::app
