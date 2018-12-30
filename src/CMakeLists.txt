cmake_minimum_required(VERSION 2.8.2 FATAL_ERROR)
set(CMAKE_LEGACY_CYGWIN_WIN32 0)
option(WITH_JAVA_SUPPORT "Enable the java build" OFF)

if(WITH_JAVA_SUPPORT)
  add_definitions(-DUSE_FEATURE_A)
  project("DevvJava" C CXX)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  #Include JNI
  include_directories(${JAVA_HOME}/include)
  include_directories(${JAVA_HOME}/include/linux)
else()
  project("Devv" C CXX)
endif()

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (Devv_VERSION_MAJOR 0)
set (Devv_VERSION_MINOR 3)

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

if(WITH_JAVA_SUPPORT)
    EXEC_PROGRAM(${PROTOBUF_PROTOC_EXECUTABLE} ARGS --proto_path ${CMAKE_CURRENT_SOURCE_DIR} --java_out ${CMAKE_CURRENT_SOURCE_DIR} pbuf/devv.proto)
endif()

# Dummy target that triggers python protobuf generation
add_custom_target(pbuf-dummy-target ALL DEPENDS ${PROTO_PY})

# Create empty variable to hold object files
set(devv_objects)

# Helper functino to register Devv libraries
function(register_devv_lib)
    add_library(${ARGV})
    set(devv_objects ${devv_objects} $<TARGET_OBJECTS:${ARGV0}> PARENT_SCOPE)
    set_property(TARGET ${ARGV0} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

# Protobuf generated library
register_devv_lib(devvpbuf OBJECT devv.pb.h devv.pb.cc)
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
file(GLOB common_SRC "common/*.cpp" "common/*.c" ${generated_SRC})
register_devv_lib(common OBJECT ${common_SRC})

# Build the concurrency module objects
file(GLOB concurrency_SRC "concurrency/*.cpp")
register_devv_lib(concurrency OBJECT ${concurrency_SRC})

# Build the consensus module objects
file(GLOB consensus_SRC "consensus/*.cpp")
register_devv_lib(consensus OBJECT ${consensus_SRC})

# Build the io module objects
file(GLOB io_SRC "io/*.cpp")
register_devv_lib(io OBJECT ${io_SRC})
add_dependencies(io devvpbuf)

# Build the pbuf module objects
file(GLOB pbuf_SRC "pbuf/*.cpp")
register_devv_lib(pbuf OBJECT ${pbuf_SRC})
add_dependencies(pbuf devvpbuf)

# Build the primitives module objects
file(GLOB primitives_SRC "primitives/*.cpp")
register_devv_lib(primitives OBJECT ${primitives_SRC})

# Add BlockchainModule.cpp
file(GLOB modules_SRC "modules/*.cpp")
register_devv_lib(modules OBJECT ${modules_SRC})
add_dependencies(modules devvpbuf)

option(WITH_THREAD_TESTING "Enable thread testing" OFF)

if(WITH_THREAD_TESTING)
  add_definitions(-DTEST_THREADS)
endif()

# Prevent gcc from mangling
# See:
#   https://stackoverflow.com/questions/41854840/var-create-unable-to-create-variable-object
#  https://youtrack.jetbrains.com/issue/CPP-8693
#add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

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
set(devv_dep_libs
        ${ZMQ}
        ${PROTOBUF_LIBRARY}
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES}
        ${PQXX}
        )

add_library(devv-core SHARED ${devv_objects})
target_link_libraries(devv-core ${devv_dep_libs})
install (TARGETS devv-core DESTINATION lib)

macro(add_devv_executable executable source_file)
    add_executable(${executable} ${source_file} ${devv_objects})
    target_link_libraries(${executable} ${devv_dep_libs})
    install(TARGETS ${executable} DESTINATION bin)
endmacro()

#
# JNI Library
#
if(WITH_JAVA_SUPPORT)
    # Uncomment the next line if you want to regenerate the jni-based header files.
    # EXEC_PROGRAM(javah -classpath ${CMAKE_CURRENT_SOURCE_DIR} -d ${CMAKE_CURRENT_SOURCE_DIR}/jni jni.devv.java)
    file(GLOB devvjni_SRC
        "jni/*.cpp"
    )
    add_library(devvjni SHARED
      ${devvjni_SRC}
    )
    install (TARGETS devvjni DESTINATION lib)
    target_link_libraries(devvjni ${devv_all_libs})
endif()


#
# executables
#
add_devv_executable(devv-validator devv-validator.cpp)
add_devv_executable(devv-scanner devv-scanner.cpp)
add_devv_executable(devv-key devv-key.cpp)
add_devv_executable(devv-sign devv-sign.cpp)
add_devv_executable(devv-announcer devv-announcer.cpp)
add_devv_executable(devv-query devv-query.cpp)
add_devv_executable(devv-psql devv-psql.cpp)
add_devv_executable(reset-db reset-db.cpp)
add_devv_executable(circuit-gen circuit-gen.cpp)
add_devv_executable(laminar-gen laminar-gen.cpp)
add_devv_executable(turbulent-gen turbulent-gen.cpp)
add_devv_executable(devv-verify devv-verify.cpp)

#
# i/o Tests
#
add_devv_executable(ring_queue_test tests/concurrency/ring_queue_test.cpp)
add_devv_executable(devv_spsc_queue_test tests/concurrency/devv_spsc_queue_test.cpp)
add_devv_executable(boost_spsc_test tests/concurrency/boost_spsc_test.cpp)
add_devv_executable(io_server_test tests/io/transaction_server.cpp)
add_devv_executable(io_client_test tests/io/transaction_client.cpp)
add_devv_executable(zmq_envelope_publisher tests/io/zmq_envelope_publisher.cpp)
add_devv_executable(zmq_envelope_subscriber tests/io/zmq_envelope_subscriber.cpp)

######### gtest ##########
include(${CMAKE_CURRENT_SOURCE_DIR}/gtest/CMakeLists.txt)
