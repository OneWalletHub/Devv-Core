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

# Primitive data type tests
add_executable(primitives_test gtest/primitives_test.cpp)
target_link_libraries(primitives_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

# unit tests for concurrency algorithms
add_executable(concurrency_test gtest/concurrency_test.cpp)
target_link_libraries(concurrency_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

# unit tests for consensus algorithms
add_executable(consensus_test gtest/consensus_test.cpp)
target_link_libraries(consensus_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

# ossladapter tests
add_executable(ossladapter_test gtest/ossladapter_test.cpp)
target_link_libraries(ossladapter_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

# protocol buffer tests
add_executable(pbuf_test gtest/pbuf_test.cpp)
target_link_libraries(pbuf_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

# blockchain tests
add_executable(blockchain_test gtest/blockchain_test.cpp)
target_link_libraries(blockchain_test ${GTEST_BOTH_LIBRARIES} ${devv_all_libs})

add_test(NAME primitives_test COMMAND primitives_test)
add_test(NAME concurrency_test COMMAND concurrency_test)
add_test(NAME consensus_test COMMAND consensus_test)
add_test(NAME ossladapter_test COMMAND ossladapter_test)
#add_test(NAME pbuf_test COMMAND pbuf_test)
add_test(NAME blockchain_test COMMAND blockchain_test)
