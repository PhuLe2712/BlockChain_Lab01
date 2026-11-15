
cmake_minimum_required(VERSION 3.20)

project(Lab01Blockchain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

add_library(blockchain_core
    src/crypto.cpp
    src/transaction.cpp
    src/state.cpp
    src/block.cpp
    src/logger.cpp
    src/network.cpp
    src/validator.cpp
    src/vote.cpp
    src/consensus.cpp
    src/distributed_node.cpp
)

target_include_directories(blockchain_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(blockchain_core
    PUBLIC OpenSSL::Crypto
)

if(MSVC)
    target_compile_options(blockchain_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(blockchain_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lab01 src/main.cpp)
target_link_libraries(lab01 PRIVATE blockchain_core)

add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE blockchain_core)

enable_testing()

add_test(NAME unit_tests COMMAND unit_tests)

add_executable(consensus_tests tests/consensus_tests.cpp)
target_link_libraries(consensus_tests PRIVATE blockchain_core)

add_test(NAME consensus_tests COMMAND consensus_tests)

add_executable(distributed_tests tests/distributed_tests.cpp)
target_link_libraries(distributed_tests PRIVATE blockchain_core)
add_test(NAME distributed_tests COMMAND distributed_tests)

add_executable(transaction_replay_tests tests/transaction_replay_tests.cpp)
target_link_libraries(transaction_replay_tests PRIVATE blockchain_core)
add_test(NAME transaction_replay_tests COMMAND transaction_replay_tests)

add_executable(network_fault_tests tests/network_fault_tests.cpp)
target_link_libraries(network_fault_tests PRIVATE blockchain_core)
add_test(NAME network_fault_tests COMMAND network_fault_tests)
