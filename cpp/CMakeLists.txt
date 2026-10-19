cmake_minimum_required(VERSION 3.16)
project(emv_integration_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(emv_warnings INTERFACE)
if(MSVC)
  target_compile_options(emv_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(emv_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

# Kernel-agnostic core: domain types, ports, orchestration and kernel plumbing. No vendor code.
add_library(emv_core STATIC
  src/domain.cpp
  src/transaction_service.cpp
  src/kernel/phased_emv_kernel.cpp
  src/kernel/kernel_registry.cpp
  src/policy/preferred_aid_selector.cpp
)
target_include_directories(emv_core PUBLIC include)
target_link_libraries(emv_core PRIVATE emv_warnings)

# Simulated adapters. A real vendor kernel gets its own adapter library next to this one.
add_library(emv_simulated STATIC
  adapters/simulated/simulated_card_reader.cpp
  adapters/simulated/simulated_contact_kernel.cpp
  adapters/simulated/simulated_contactless_kernel.cpp
  adapters/simulated/stub_host_authorizer.cpp
  adapters/simulated/register_simulated_kernels.cpp
)
target_include_directories(emv_simulated PUBLIC adapters)
target_link_libraries(emv_simulated PUBLIC emv_core PRIVATE emv_warnings)

add_executable(emv_demo app/main.cpp)
target_link_libraries(emv_demo PRIVATE emv_simulated emv_warnings)

enable_testing()
foreach(test_name transaction_service_test phased_emv_kernel_test kernel_registry_test)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE emv_core emv_warnings)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
