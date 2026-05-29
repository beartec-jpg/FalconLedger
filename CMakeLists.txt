cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

# Fix "unrecognized escape" issues when passing CMAKE_MODULE_PATH on Windows.
if(DEFINED CMAKE_MODULE_PATH)
    file(TO_CMAKE_PATH "${CMAKE_MODULE_PATH}" CMAKE_MODULE_PATH)
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(xrpl)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CompilationEnv)

if(is_gcc)
    # GCC-specific fixes
    add_compile_options(-Wno-unknown-pragmas -Wno-subobject-linkage)
    # -Wno-subobject-linkage can be removed when we upgrade GCC version to at least 13.3
elseif(is_clang)
    # Clang-specific fixes
    add_compile_options(-Wno-unknown-warning-option) # Ignore unknown warning options
elseif(is_msvc)
    # MSVC-specific fixes
    add_compile_options(/wd4068) # Ignore unknown pragmas
endif()

# Enable ccache to speed up builds.
include(Ccache)

if(thread_safety_analysis)
    add_compile_options(
        -Wthread-safety
        -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS
        -DXRPL_ENABLE_THREAD_SAFETY_ANNOTATIONS
    )
    add_compile_options("-stdlib=libc++")
    add_link_options("-stdlib=libc++")
endif()

include(CheckCXXCompilerFlag)
include(FetchContent)
include(ExternalProject)
include(CMakeFuncs) # must come *after* ExternalProject b/c it overrides one function in EP
if(target)
    message(
        FATAL_ERROR
        "The target option has been removed - use native cmake options to control build"
    )
endif()

include(XrplSanity)
include(XrplVersion)
include(XrplSettings)
# this check has to remain in the top-level cmake because of the early return statement
if(packages_only)
    if(NOT TARGET rpm)
        message(
            FATAL_ERROR
            "packages_only requested, but targets were not created - is docker installed?"
        )
    endif()
    return()
endif()
include(XrplCompiler)
include(XrplSanitizers)
include(XrplInterface)

option(only_docs "Include only the docs target?" FALSE)
include(XrplDocs)
if(only_docs)
    return()
endif()

include(deps/Boost)

add_subdirectory(external/antithesis-sdk)
find_package(date REQUIRED)
find_package(ed25519 REQUIRED)
find_package(gRPC REQUIRED)
find_package(LibArchive REQUIRED)
find_package(lz4 REQUIRED)
find_package(nudb REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(secp256k1 REQUIRED)
find_package(SOCI REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(xxHash REQUIRED)

# liboqs: required for post-quantum Falcon signatures.
# qXRP is a PQ-first chain — Falcon is always enabled.
#
# SECURITY NOTE (H-01 from 2026 security audit):
# We pin to an exact commit SHA (not just a tag) to mitigate supply-chain risk.
# Falcon signatures are used for validator identity and consensus participation.
# Changing this commit requires careful review and should be documented in release notes.
find_package(liboqs QUIET)
if(NOT liboqs_FOUND)
    message(STATUS "liboqs not found via find_package, building from source")
    include(ExternalProject)
    set(LIBOQS_INSTALL_DIR ${CMAKE_BINARY_DIR}/liboqs-install)

    # Pinned commit for liboqs v0.12.0 (https://github.com/open-quantum-safe/liboqs/releases/tag/0.12.0)
    # This is the exact tree used for reproducible builds of qXRP.
    set(LIBOQS_PINNED_COMMIT f4b96220e4bd208895172acc4fedb5a191d9f5b1)

    ExternalProject_Add(
        liboqs_external
        GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
        GIT_TAG        ${LIBOQS_PINNED_COMMIT}
        GIT_SHALLOW    TRUE
        UPDATE_DISCONNECTED TRUE
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${LIBOQS_INSTALL_DIR}
            -DBUILD_SHARED_LIBS=OFF
            -DOQS_BUILD_ONLY_LIB=ON
            -DOQS_ALGS_ENABLED=Falcon
            -DOQS_PERMIT_UNSUPPORTED_ARCHITECTURE=ON
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        BUILD_BYPRODUCTS ${LIBOQS_INSTALL_DIR}/lib/liboqs.a
    )
    file(MAKE_DIRECTORY ${LIBOQS_INSTALL_DIR}/include)
    add_library(OQS::oqs STATIC IMPORTED GLOBAL)
    set_target_properties(OQS::oqs PROPERTIES
        IMPORTED_LOCATION ${LIBOQS_INSTALL_DIR}/lib/liboqs.a
        INTERFACE_INCLUDE_DIRECTORIES ${LIBOQS_INSTALL_DIR}/include
    )
    add_dependencies(OQS::oqs liboqs_external)
endif()
message(STATUS "liboqs: PQ Falcon signatures always enabled (pinned commit: ${LIBOQS_PINNED_COMMIT})")

# Security note for developers (M-01 from 2026 audit):
# When working on qXRP changes (especially Falcon, rewards, slashing, governance),
# it is strongly recommended to build with sanitizers:
#   cmake .. -DSANITIZE=address,undefined
# Then run the test suite. This helps catch memory safety and undefined behavior
# in the new consensus-critical code paths.
#
# For focused qXRP development, you can also limit the build:
#   cmake .. -Dxrpld=ON -Dtests=ON -DSANITIZE=address,undefined
add_compile_definitions(XRPL_HAS_LIBOQS)
target_link_libraries(xrpl_libs INTERFACE OQS::oqs)

target_link_libraries(
    xrpl_libs
    INTERFACE
        ed25519::ed25519
        lz4::lz4
        OpenSSL::Crypto
        OpenSSL::SSL
        secp256k1::secp256k1
        soci::soci
        SQLite::SQLite3
)

option(rocksdb "Enable RocksDB" ON)
if(rocksdb)
    find_package(RocksDB REQUIRED)
    set_target_properties(
        RocksDB::rocksdb
        PROPERTIES INTERFACE_COMPILE_DEFINITIONS XRPL_ROCKSDB_AVAILABLE=1
    )
    target_link_libraries(xrpl_libs INTERFACE RocksDB::rocksdb)
endif()

# Work around changes to Conan recipe for now.
if(TARGET nudb::core)
    set(nudb nudb::core)
elseif(TARGET NuDB::nudb)
    set(nudb NuDB::nudb)
else()
    message(FATAL_ERROR "unknown nudb target")
endif()
target_link_libraries(xrpl_libs INTERFACE ${nudb})

if(coverage)
    include(XrplCov)
endif()

include(XrplCore)
include(XrplProtocolAutogen)
include(XrplInstall)
include(XrplValidatorKeys)

if(tests)
    include(CTest)
    add_subdirectory(src/tests/libxrpl)
endif()
