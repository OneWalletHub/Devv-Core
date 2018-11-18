cmake_minimum_required(VERSION 2.8.2 FATAL_ERROR)
set(CMAKE_LEGACY_CYGWIN_WIN32 0)

project("Devv" C CXX)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (Devv_VERSION_MAJOR 0)
set (Devv_VERSION_MINOR 2)

enable_testing()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(warnings "-Wall -Wextra \
                  -pedantic \
                  -Wlogical-op \
                  -Wno-unused-function \
                  -Wno-unused-parameter \
                  -Wno-vla \
                  -fdiagnostics-show-option")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(warnings "/W4 /WX /EHsc")
endif()

if (NOT CONFIGURED_ONCE)
    set(CMAKE_CXX_FLAGS "${warnings} -Woverloaded-virtual -Werror -std=gnu++11"
        CACHE STRING "Flags used by the compiler during all build types." FORCE)
    set(CMAKE_C_FLAGS   "${warnings}"
        CACHE STRING "Flags used by the compiler during all build types." FORCE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

# External third-party libraries
find_library(ZMQ zmq)
find_library(PQXX pqxx)

### protobuf defs ###
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS pbuf/devv.proto)
protobuf_generate_python(PROTO_PY pbuf/devv.proto)

# Dummy target that triggers python protobuf generation
add_custom_target(myTarget ALL DEPENDS ${PROTO_PY})

file(GLOB_RECURSE dcSources "*.cpp")
file(GLOB_RECURSE dcHeaders "*.h*")
set (dcInclude "")
foreach (_headerFile ${dcHeaders})
  get_filename_component(_dir ${_headerFile} PATH)
  list (APPEND dcInclude ${_dir})
endforeach()
list(REMOVE_DUPLICATES dcInclude)

# Protobuf generated library
ADD_LIBRARY(devvpbuf devv.pb.h devv.pb.cc)
set_source_files_properties(devv.pb.h devv.pb.cc PROPERTIES GENERATED TRUE)

find_package(Git)
# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(COMMAND
        "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=7 --dirty
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_SHA1
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND
        "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_DATE
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the subject of the commit
execute_process(COMMAND
        "${GIT_EXECUTABLE}" log -1 --format=%s
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the current working branch
execute_process(COMMAND
        "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# generate version.cc
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/common/devv_version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/devv_version.cpp" @ONLY)

list(APPEND generated_SRC "${CMAKE_CURRENT_BINARY_DIR}/devv_version.cpp" devv_version.h)

# Build the common module objects
file(GLOB common_SRC
    "common/*.cpp" "common/*.c" ${generated_SRC}
)
add_library(common ${common_SRC})

# Build the concurrency module objects
file(GLOB concurrency_SRC
    "concurrency/*.cpp"
)
add_library(concurrency ${concurrency_SRC})

# Build the consensus module objects
file(GLOB consensus_SRC
    "consensus/*.cpp"
)
add_library(consensus ${consensus_SRC})

# Build the io module objects
file(GLOB io_SRC
    "io/*.cpp"
)
add_library(io
  ${io_SRC}
  )

# Build the primitives module objects
file(GLOB primitives_SRC
    "primitives/*.cpp"
)
add_library(primitives
  ${primitives_SRC}
  )

# Add BlockchainModule.cpp
add_library(modules modules/BlockchainModule.cpp)
add_dependencies(modules devvpbuf)

set(devv_core_libs modules common concurrency consensus io primitives devvpbuf)

option(WITH_THREAD_TESTING "Enable thread testing" OFF)

if(WITH_THREAD_TESTING)
  add_definitions(-DTEST_THREADS)
endif()

# Find OpenSSL and setup variables
find_package (OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Find thread library and setup variables
find_package(Threads REQUIRED)

# Find boost and setup variables
find_package(Boost COMPONENTS date_time filesystem thread
  system program_options log REQUIRED)

set(CONFIGURED_ONCE TRUE CACHE INTERNAL
    "A flag showing that CMake has configured at least once.")

# Convenience variable containing all linker libs
set(devv_all_libs
        ${devv_core_libs}
        ${ZMQ}
        ${PROTOBUF_LIBRARY}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES}
        ${PQXX}
        )

#
# devv executable
#
add_executable(devv-validator devv-validator.cpp)
target_link_libraries (devv-validator ${devv_all_libs})
target_include_directories(devv-validator PRIVATE ${dcInclude})
install (TARGETS devv-validator DESTINATION bin)

#
# devv-scanner
#
add_executable(devv-scanner devv-scanner.cpp ${dcInclude})
target_link_libraries (devv-scanner ${devv_all_libs})
target_include_directories(devv-scanner PRIVATE ${dcInclude})
install (TARGETS devv-scanner DESTINATION bin)

#
# devv-key
#
add_executable(devv-key devv-key.cpp ${dcInclude} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries (devv-key ${devv_all_libs})
target_include_directories(devv-key PRIVATE ${dcInclude})
install (TARGETS devv-key DESTINATION bin)

#
# devv-sign
#
add_executable(devv-sign devv-sign.cpp ${dcInclude} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries (devv-sign ${devv_all_libs})
target_include_directories(devv-sign PRIVATE ${dcInclude})
install (TARGETS devv-sign DESTINATION bin)

#
# devv-annnouncer
#
add_executable(devv-announcer devv-announcer.cpp ${dcInclude} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries (devv-announcer ${devv_all_libs})
target_include_directories(devv-announcer PRIVATE ${dcInclude})
install (TARGETS devv-announcer DESTINATION bin)

#
# devv-query
#
add_executable(devv-query devv-query.cpp ${dcInclude})
target_link_libraries (devv-query ${devv_all_libs})
target_include_directories(devv-query PRIVATE ${dcInclude})
install (TARGETS devv-query DESTINATION bin)

#
# devv-psql
#
add_executable(devv-psql devv-psql.cpp ${dcInclude})
target_link_libraries (devv-psql ${devv_all_libs})
target_include_directories(devv-psql PRIVATE ${dcInclude})
install (TARGETS devv-psql DESTINATION bin)

#
# reset-db 
#

add_executable(reset-db reset-db.cpp ${dcInclude})
target_link_libraries (reset-db ${devv_all_libs})
target_include_directories(reset-db PRIVATE ${dcInclude})
install (TARGETS reset-db DESTINATION bin)

#
# circuit-gen
#
add_executable(circuit-gen circuit-gen.cpp ${dcInclude})
target_link_libraries (circuit-gen ${devv_all_libs})
target_include_directories(circuit-gen PRIVATE ${dcInclude})
install (TARGETS circuit-gen DESTINATION bin)

#
# laminar-gen
#
add_executable(laminar-gen laminar-gen.cpp ${dcInclude})
target_link_libraries (laminar-gen ${devv_all_libs})
target_include_directories(laminar-gen PRIVATE ${dcInclude})
install (TARGETS laminar-gen DESTINATION bin)

#
# turbulent-gen
#
add_executable(turbulent-gen turbulent-gen.cpp ${dcInclude})
target_link_libraries (turbulent-gen ${devv_all_libs})
target_include_directories(turbulent-gen PRIVATE ${dcInclude})
install (TARGETS turbulent-gen DESTINATION bin)

#
# devv-verify
#
add_executable(devv-verify devv-verify.cpp ${dcInclude})
target_link_libraries (devv-verify ${devv_all_libs})
target_include_directories(devv-verify PRIVATE ${dcInclude})
install (TARGETS devv-verify DESTINATION bin)

#
# i/o Tests
#
add_executable(ring_queue_test
  tests/concurrency/ring_queue_test.cpp
  )

target_link_libraries(ring_queue_test
  ${devv_core_libs}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  )

install (TARGETS ring_queue_test DESTINATION bin)

# # # devv_spsc_queue_test
add_executable(devv_spsc_queue_test
  tests/concurrency/devv_spsc_queue_test.cpp
  )

target_link_libraries(devv_spsc_queue_test
  ${devv_core_libs}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  )

install (TARGETS devv_spsc_queue_test DESTINATION bin)

# # # boost_spsc_test
add_executable(boost_spsc_test
  tests/concurrency/boost_spsc_test.cpp
  )

target_link_libraries(boost_spsc_test
  ${devv_core_libs}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  )

install (TARGETS boost_spsc_test DESTINATION bin)

# # # io_server_test
add_executable(io_server_test
  tests/io/transaction_server.cpp
  )

target_link_libraries(io_server_test
  ${devv_core_libs}
  ${ZMQ}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  )

# # # io_client_test
add_executable(io_client_test
  tests/io/transaction_client.cpp
  )

target_link_libraries(io_client_test
  ${devv_core_libs}
  ${ZMQ}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_LIBRARIES}
  )

# # # zmq_envelope_publisher
add_executable(zmq_envelope_publisher
  tests/io/zmq_envelope_publisher.cpp
  )

target_link_libraries(zmq_envelope_publisher
  ${ZMQ}
  )

# # # zmq_envelope_subscriber
add_executable(zmq_envelope_subscriber
  tests/io/zmq_envelope_subscriber.cpp
  )

target_link_libraries(zmq_envelope_subscriber
  ${ZMQ}
  )

######### gtest ##########
include(${CMAKE_CURRENT_SOURCE_DIR}/gtest/CMakeLists.txt)
