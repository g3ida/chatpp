if(WIN32)
  set(CHATTPP_WINSOCK_LIBRARY ws2_32 wsock32)
endif()

option(CHATPP_ENABLE_TLS "Enable TLS support" ON)
option(CHATPP_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(CHATPP_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

add_executable(chatpp main.cpp server.cpp client.cpp message.cpp tls.cpp)

target_include_directories(chatpp SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/extern)
target_link_libraries(chatpp PRIVATE Threads::Threads ${CHATTPP_WINSOCK_LIBRARY})

if(CHATPP_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(chatpp PRIVATE /W4)
  else()
    target_compile_options(chatpp PRIVATE -Wall -Wextra)
  endif()
endif()

if(CHATPP_ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "CHATPP_ENABLE_SANITIZERS is not supported with MSVC")
  endif()
  target_compile_options(chatpp PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(chatpp PRIVATE -fsanitize=address,undefined)
endif()

if(CHATPP_ENABLE_TLS)
  find_package(OpenSSL REQUIRED)
  target_compile_definitions(chatpp PRIVATE CHATPP_ENABLE_TLS)
  target_link_libraries(chatpp PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
