set( PROJECT_NAME chatpp )

project(chatpp)

if(WIN32)
  set(CHATTPP_WINSOCK_LIBRARY ws2_32 wsock32)
endif()

if(UNIX)
  set(PTHREAD_LIBRARY pthread)
endif()

option(CHATPP_ENABLE_TLS "Enable TLS support" ON)

add_executable(chatpp main.cpp server.cpp client.cpp message.cpp tls.cpp)

target_link_libraries(chatpp PRIVATE ${CHATTPP_WINSOCK_LIBRARY} ${PTHREAD_LIBRARY})

if(CHATPP_ENABLE_TLS)
  find_package(OpenSSL REQUIRED)
  target_compile_definitions(chatpp PRIVATE CHATPP_ENABLE_TLS)
  target_include_directories(chatpp PRIVATE ${OPENSSL_INCLUDE_DIR})
  target_link_libraries(chatpp PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
endif()
