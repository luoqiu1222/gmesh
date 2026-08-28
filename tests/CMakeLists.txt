add_executable(gmesh_core_tests test_repair.cpp)
target_compile_features(gmesh_core_tests PRIVATE cxx_std_17)
target_link_libraries(gmesh_core_tests PRIVATE gmesh::core)

add_test(NAME gmesh.core COMMAND gmesh_core_tests)
add_test(NAME gmesh.cli.help COMMAND $<TARGET_FILE:gmesh> --help)
set_tests_properties(gmesh.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION "Independent triangle-mesh repair CLI")
add_test(NAME gmesh.cli.version COMMAND $<TARGET_FILE:gmesh> --version)
set_tests_properties(gmesh.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "gmesh 0.3.0")
