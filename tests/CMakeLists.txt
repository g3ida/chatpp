add_executable(chatpp_tests security_tests.cpp)
target_include_directories(chatpp_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME chatpp_tests COMMAND chatpp_tests)
