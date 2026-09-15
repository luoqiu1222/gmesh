add_executable(gmesh_core_tests test_repair.cpp)
target_compile_features(gmesh_core_tests PRIVATE cxx_std_17)
target_link_libraries(gmesh_core_tests PRIVATE gmesh::core)
target_compile_definitions(gmesh_core_tests PRIVATE
    GMESH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test_data")

add_test(NAME gmesh.core COMMAND gmesh_core_tests)
add_executable(gmesh_curved_hole_tests test_curved_hole.cpp)
target_compile_features(gmesh_curved_hole_tests PRIVATE cxx_std_17)
target_link_libraries(gmesh_curved_hole_tests PRIVATE gmesh::core)
add_test(NAME gmesh.curved_hole COMMAND gmesh_curved_hole_tests)
add_executable(gmesh_large_hole_tests test_large_hole.cpp)
target_compile_features(gmesh_large_hole_tests PRIVATE cxx_std_17)
target_link_libraries(gmesh_large_hole_tests PRIVATE gmesh::core)
add_test(NAME gmesh.large_hole COMMAND gmesh_large_hole_tests)
set_tests_properties(gmesh.large_hole PROPERTIES TIMEOUT 20)
add_executable(gmesh_orientation_tests test_orientation.cpp)
target_compile_features(gmesh_orientation_tests PRIVATE cxx_std_17)
target_link_libraries(gmesh_orientation_tests PRIVATE gmesh::core)
target_compile_definitions(gmesh_orientation_tests PRIVATE
    GMESH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test_data")
add_test(NAME gmesh.orientation COMMAND gmesh_orientation_tests)
add_test(NAME gmesh.cli.help COMMAND $<TARGET_FILE:gmesh> --help)
set_tests_properties(gmesh.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION "Independent triangle-mesh repair CLI")
add_test(NAME gmesh.cli.version COMMAND $<TARGET_FILE:gmesh> --version)
set_tests_properties(gmesh.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "gmesh 0.3.0")
