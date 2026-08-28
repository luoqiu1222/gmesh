add_library(gmesh_core STATIC repair.cpp)
add_library(gmesh::core ALIAS gmesh_core)

target_compile_features(gmesh_core PUBLIC cxx_std_17)
target_include_directories(gmesh_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_executable(gmesh main.cpp)
target_compile_features(gmesh PRIVATE cxx_std_17)
target_link_libraries(gmesh PRIVATE gmesh::core)

if(MSVC)
    target_compile_options(gmesh_core PRIVATE /W4 /permissive-)
    target_compile_options(gmesh PRIVATE /W4 /permissive-)
    if(GMESH_WARNINGS_AS_ERRORS)
        target_compile_options(gmesh_core PRIVATE /WX)
        target_compile_options(gmesh PRIVATE /WX)
    endif()
else()
    target_compile_options(gmesh_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gmesh PRIVATE -Wall -Wextra -Wpedantic)
    if(GMESH_WARNINGS_AS_ERRORS)
        target_compile_options(gmesh_core PRIVATE -Werror)
        target_compile_options(gmesh PRIVATE -Werror)
    endif()
endif()

install(TARGETS gmesh gmesh_core
        EXPORT gmeshTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
