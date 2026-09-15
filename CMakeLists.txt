cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0167 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0144 NEW)

option(GMESH_MACOS_UNIVERSAL "Build a universal arm64+x86_64 binary on macOS" ON)
set(GMESH_MACOS_DEPLOYMENT_TARGET "12.0" CACHE STRING
    "Minimum supported macOS version")

# These cache variables must be initialized before project() selects a compiler.
# Callers may override either value explicitly when a single-architecture or
# different deployment target build is required. CMake ignores these OSX cache
# variables on non-Apple platforms, so setting their defaults unconditionally
# avoids relying on host variables that are initialized by project().
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET
        "${GMESH_MACOS_DEPLOYMENT_TARGET}" CACHE STRING
        "Minimum supported macOS version" FORCE)
endif()
if(GMESH_MACOS_UNIVERSAL AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "macOS target architectures" FORCE)
endif()

project(gmesh
    VERSION 0.3.0
    DESCRIPTION "Independent command-line tool for triangle-mesh repair"
    LANGUAGES C CXX
)

option(GMESH_BUILD_TESTS "Build gmesh tests" ON)
option(GMESH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(GMESH_FETCH_DEPENDENCIES "Download pinned ADMesh and CGAL sources" ON)
set(GMESH_ADMESH_SOURCE_DIR "" CACHE PATH "Path to an ADMesh 0.98.5 source tree")
set(GMESH_CGAL_SOURCE_DIR "" CACHE PATH "Path to a CGAL 5.6.3 source tree")
set(GMESH_BOOST_SOURCE_DIR "" CACHE PATH "Path to a Boost 1.84.0 source tree")

set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(FetchContent)

if(NOT GMESH_ADMESH_SOURCE_DIR)
    if(NOT GMESH_FETCH_DEPENDENCIES)
        message(FATAL_ERROR "Set GMESH_ADMESH_SOURCE_DIR or enable GMESH_FETCH_DEPENDENCIES")
    endif()
    FetchContent_Declare(admesh_source
        GIT_REPOSITORY https://github.com/admesh/admesh.git
        GIT_TAG 70ca24a9b4e6d8aa05e8572e768110dad9b4d47b
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(admesh_source)
    if(NOT admesh_source_POPULATED)
        FetchContent_Populate(admesh_source)
    endif()
    set(GMESH_ADMESH_SOURCE_DIR "${admesh_source_SOURCE_DIR}")
endif()

add_library(gmesh_admesh STATIC
    "${GMESH_ADMESH_SOURCE_DIR}/src/connect.c"
    "${GMESH_ADMESH_SOURCE_DIR}/src/normals.c"
    "${GMESH_ADMESH_SOURCE_DIR}/src/shared.c"
    "${GMESH_ADMESH_SOURCE_DIR}/src/stl_io.c"
    "${GMESH_ADMESH_SOURCE_DIR}/src/stlinit.c"
    "${GMESH_ADMESH_SOURCE_DIR}/src/util.c"
)
target_include_directories(gmesh_admesh SYSTEM PUBLIC "${GMESH_ADMESH_SOURCE_DIR}/src")
target_include_directories(gmesh_admesh BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/admesh-compat")
set_target_properties(gmesh_admesh PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_definitions(gmesh_admesh PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
endif()

find_package(Boost 1.84.0 EXACT QUIET)
if(NOT Boost_FOUND)
    if(NOT GMESH_BOOST_SOURCE_DIR)
        if(NOT GMESH_FETCH_DEPENDENCIES)
            message(FATAL_ERROR "Install Boost, set GMESH_BOOST_SOURCE_DIR, or enable GMESH_FETCH_DEPENDENCIES")
        endif()
        FetchContent_Declare(boost_source
            URL https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.bz2
            URL_HASH SHA256=cc4b893acf645c9d4b698e9a0f08ca8846aa5d6c68275c14c3e7949c24109454
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_GetProperties(boost_source)
        if(NOT boost_source_POPULATED)
            FetchContent_Populate(boost_source)
        endif()
        set(GMESH_BOOST_SOURCE_DIR "${boost_source_SOURCE_DIR}")
    endif()
    set(BOOST_ROOT "${GMESH_BOOST_SOURCE_DIR}")
    set(Boost_INCLUDE_DIR "${GMESH_BOOST_SOURCE_DIR}" CACHE PATH "Boost headers" FORCE)
    find_package(Boost 1.84.0 EXACT REQUIRED)
endif()

set(CGAL_DISABLE_GMP ON CACHE BOOL "Use CGAL without GMP" FORCE)
set(CGAL_DISABLE_MPFR ON CACHE BOOL "Use CGAL without MPFR" FORCE)
find_package(CGAL 5.6.3 EXACT QUIET)
if(NOT CGAL_FOUND)
    if(NOT GMESH_CGAL_SOURCE_DIR)
        if(NOT GMESH_FETCH_DEPENDENCIES)
            message(FATAL_ERROR "Install CGAL 5.6.3, set GMESH_CGAL_SOURCE_DIR, or enable GMESH_FETCH_DEPENDENCIES")
        endif()
        FetchContent_Declare(cgal_source
            URL https://github.com/CGAL/cgal/releases/download/v5.6.3/CGAL-5.6.3.zip
            URL_HASH SHA256=5d577acb4a9918ccb960491482da7a3838f8d363aff47e14d703f19fd84733d4
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_GetProperties(cgal_source)
        if(NOT cgal_source_POPULATED)
            FetchContent_Populate(cgal_source)
        endif()
        set(GMESH_CGAL_SOURCE_DIR "${cgal_source_SOURCE_DIR}")
    endif()
    set(CGAL_DIR "${GMESH_CGAL_SOURCE_DIR}" CACHE PATH "CGAL package directory" FORCE)
    find_package(CGAL 5.6.3 EXACT REQUIRED)
endif()

add_subdirectory(src)

if(GMESH_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
    add_test(NAME gmesh.licenses
             COMMAND "${CMAKE_COMMAND}" -P
                     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_licenses.cmake")
endif()

install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gmesh")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gmesh")
