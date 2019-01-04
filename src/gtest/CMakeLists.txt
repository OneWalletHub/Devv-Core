# gtest/CMakeLists.txt
#
#    Date: Jun 23 2018
#  Author: Shawn McKenney <shawn.mckenney@emmion.com>
#
# Enable testing, configure GoogleTest and GoogleMock
# and compile tests
#

enable_testing()
find_package(GTest REQUIRED)
#find_package(GMock REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

macro(add_gtest executable source_file)
    add_executable(${executable} ${source_file} ${devv_objects})
    target_link_libraries(${executable} ${GTEST_BOTH_LIBRARIES} ${devv_dep_libs})
    add_test(NAME ${executable} COMMAND ${executable})
endmacro()

# Primitive data type tests
add_gtest(primitives_test gtest/primitives_test.cpp)

# unit tests for concurrency algorithms
add_gtest(concurrency_test gtest/concurrency_test.cpp)

# unit tests for consensus algorithms
add_gtest(consensus_test gtest/consensus_test.cpp)

# ossladapter tests
add_gtest(ossladapter_test gtest/ossladapter_test.cpp)

# protocol buffer tests
#add_gtest(pbuf_test gtest/pbuf_test.cpp)
#add_dependencies(pbuf_test devvpbuf)

# blockchain tests
add_gtest(blockchain_test gtest/blockchain_test.cpp)

# blockchainmodule tests
add_gtest(blockchainmodule_test gtest/blockchainmodule_test.cpp)
