#include "emv/kernel/kernel_registry.hpp"
#include <stdexcept>
#include <utility>

namespace emv {

void KernelRegistry::add(const std::string& name, Factory factory) {
    if (name.empty()) throw std::invalid_argument("Kernel name is required");
    if (!factory) throw std::invalid_argument("Kernel factory is required: " + name);
    if (!factories_.emplace(name, std::move(factory)).second) {
        throw std::invalid_argument("Kernel already registered: " + name);
    }
}

std::unique_ptr<IEmvKernel> KernelRegistry::create(const std::string& name) const {
    const auto entry = factories_.find(name);
    if (entry == factories_.end()) {
        std::string available;
        for (const auto& registered : names()) {
            available += (available.empty() ? "" : ", ") + registered;
        }
        throw std::out_of_range("Unknown kernel '" + name + "'. Available: " + available);
    }
    auto kernel = entry->second();
    if (!kernel) throw std::runtime_error("Kernel factory returned null: " + name);
    return kernel;
}

bool KernelRegistry::contains(const std::string& name) const {
    return factories_.count(name) != 0;
}

std::vector<std::string> KernelRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(factories_.size());
    for (const auto& entry : factories_) result.push_back(entry.first);
    return result;
}

} // namespace emv
