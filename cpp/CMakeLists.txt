cmake_minimum_required(VERSION 3.16)
project(emv_integration_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EMV_BUILD_TERMINAL_AGENT "Build the HTTP terminal agent (downloads cpp-httplib and nlohmann/json)" ON)

add_library(emv_warnings INTERFACE)
if(MSVC)
  target_compile_options(emv_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(emv_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

# Kernel-agnostic core: domain types, ports, orchestration and kernel plumbing. No vendor code.
add_library(emv_core STATIC
  src/domain.cpp
  src/tlv.cpp
  src/emv_tags.cpp
  src/crypto/sha1.cpp
  src/config/config_file.cpp
  src/config/emv_configuration.cpp
  src/transaction_service.cpp
  src/kernel/phased_emv_kernel.cpp
  src/kernel/kernel_registry.cpp
  src/policy/preferred_aid_selector.cpp
)
target_include_directories(emv_core PUBLIC include)
target_link_libraries(emv_core PRIVATE emv_warnings)

# Simulated adapters. A real vendor kernel gets its own adapter library next to this one.
add_library(emv_simulated STATIC
  adapters/simulated/simulation_options.cpp
  adapters/simulated/simulated_cards.cpp
  adapters/simulated/mock_issuer_crypto.cpp
  adapters/simulated/simulated_emv_engine.cpp
  adapters/simulated/simulated_card_reader.cpp
  adapters/simulated/simulated_contact_kernel.cpp
  adapters/simulated/simulated_contactless_kernel.cpp
  adapters/simulated/stub_host_authorizer.cpp
  adapters/simulated/register_simulated_kernels.cpp
)
target_include_directories(emv_simulated PUBLIC adapters)
target_link_libraries(emv_simulated PUBLIC emv_core PRIVATE emv_warnings)

# Wiring shared by the composition roots (which kernels this build offers).
add_library(emv_app_common STATIC app/common/kernel_catalog.cpp)
target_include_directories(emv_app_common PUBLIC app)
target_link_libraries(emv_app_common PUBLIC emv_simulated PRIVATE emv_warnings)
# The EMV parameter files (TERMINAL.TXT, AIDS_CONTACT.TXT, AIDS_CONTACTLESS.TXT, CAKEYS.TXT).
set(EMV_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_compile_definitions(emv_app_common PRIVATE EMV_DEFAULT_CONFIG_DIR="${EMV_CONFIG_DIR}")

add_executable(emv_demo app/cli/main.cpp)
target_link_libraries(emv_demo PRIVATE emv_app_common emv_warnings)

enable_testing()
foreach(test_name transaction_service_test phased_emv_kernel_test kernel_registry_test emv_config_test)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE emv_core emv_warnings)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
add_executable(simulated_kernel_test tests/simulated_kernel_test.cpp)
target_link_libraries(simulated_kernel_test PRIVATE emv_simulated emv_warnings)
add_test(NAME simulated_kernel_test COMMAND simulated_kernel_test)
target_compile_definitions(emv_config_test PRIVATE EMV_CONFIG_DIR="${EMV_CONFIG_DIR}")
target_compile_definitions(simulated_kernel_test PRIVATE EMV_CONFIG_DIR="${EMV_CONFIG_DIR}")

if(EMV_BUILD_TERMINAL_AGENT)
  if(CMAKE_VERSION VERSION_LESS 3.25)
    message(FATAL_ERROR "The terminal agent needs CMake 3.25+; configure with -DEMV_BUILD_TERMINAL_AGENT=OFF to skip it")
  endif()

  include(FetchContent)
  set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.22.0.tar.gz
    URL_HASH SHA256=fcfea48c8f2c386e7085ef8545c8a4875efa30fa6d5cf9dd31f03c6ad038da9d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
  FetchContent_MakeAvailable(httplib nlohmann_json)

  # Host authorizer that calls the Spring Boot authorization API over HTTP.
  add_library(emv_http STATIC adapters/http/http_host_authorizer.cpp)
  target_include_directories(emv_http PUBLIC adapters)
  target_link_libraries(emv_http PUBLIC emv_core PRIVATE httplib::httplib nlohmann_json::nlohmann_json emv_warnings)

  add_library(emv_agent_protocol STATIC app/terminal_agent/agent_protocol.cpp)
  target_link_libraries(emv_agent_protocol
    PUBLIC emv_app_common nlohmann_json::nlohmann_json PRIVATE emv_warnings)

  add_executable(emv_terminal_agent
    app/terminal_agent/main.cpp
    app/terminal_agent/terminal_agent.cpp
    app/terminal_agent/ndjson_event_stream.cpp
  )
  target_link_libraries(emv_terminal_agent PRIVATE emv_agent_protocol emv_http httplib::httplib emv_warnings)

  add_executable(agent_protocol_test tests/agent_protocol_test.cpp)
  target_link_libraries(agent_protocol_test PRIVATE emv_agent_protocol emv_warnings)
  add_test(NAME agent_protocol_test COMMAND agent_protocol_test)
endif()
