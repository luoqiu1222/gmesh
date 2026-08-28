cmake_minimum_required(VERSION 3.20)

option(GMESH_MACOS_UNIVERSAL "Build a universal arm64+x86_64 binary on macOS" ON)
set(GMESH_MACOS_DEPLOYMENT_TARGET "12.0" CACHE STRING
    "Minimum supported macOS version")

# These cache variables must be initialized before project() selects a compiler.
# Callers may override either value explicitly when a single-architecture or
# different deployment target build is required.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET
            "${GMESH_MACOS_DEPLOYMENT_TARGET}" CACHE STRING
            "Minimum supported macOS version")
    endif()
    if(GMESH_MACOS_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS target architectures")
    endif()
endif()

project(gmesh
    VERSION 0.1.0
    DESCRIPTION "Independent command-line tool for triangle-mesh repair"
    LANGUAGES CXX
)

option(GMESH_BUILD_TESTS "Build gmesh tests" ON)
option(GMESH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
add_subdirectory(src)

if(GMESH_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
    add_test(NAME gmesh.licenses
             COMMAND "${CMAKE_COMMAND}" -P
                     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_licenses.cmake")
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/gmesh"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gmesh")
