#pragma once
#include "emv/ports/emv_kernel.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace emv {

// Maps kernel names to factories so the kernel can be chosen at runtime (config, CLI,
// per entry mode) without the core knowing any concrete kernel. Each adapter library
// exposes a register function that adds its kernels here.
class KernelRegistry {
public:
    using Factory = std::function<std::unique_ptr<IEmvKernel>()>;

    // Throws std::invalid_argument if the name is empty, already taken, or the factory is empty.
    void add(const std::string& name, Factory factory);

    // Throws std::out_of_range, listing the registered names, if the kernel is unknown.
    std::unique_ptr<IEmvKernel> create(const std::string& name) const;

    bool contains(const std::string& name) const;
    std::vector<std::string> names() const;

private:
    std::map<std::string, Factory> factories_;
};

} // namespace emv
