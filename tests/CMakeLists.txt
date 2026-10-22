add_executable(chatpp_tests security_tests.cpp)
target_include_directories(chatpp_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(chatpp_tests PRIVATE Threads::Threads)
add_test(NAME chatpp_tests COMMAND chatpp_tests)
