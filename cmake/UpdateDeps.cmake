cmake_minimum_required(VERSION 3.20)

get_filename_component(EXTERN_DIR "${CMAKE_CURRENT_LIST_DIR}/../extern" ABSOLUTE)

if(NOT DEFINED WORK_DIR)
  if(DEFINED ENV{TMPDIR})
    set(WORK_DIR "$ENV{TMPDIR}/chatpp-deps")
  elseif(DEFINED ENV{TEMP})
    set(WORK_DIR "$ENV{TEMP}/chatpp-deps")
  else()
    set(WORK_DIR "${CMAKE_CURRENT_LIST_DIR}/../build/deps")
  endif()
endif()

set(ASIO_VERSION "1.32.0")
set(WEBSOCKETPP_VERSION "0.8.2")
set(JSON_VERSION "3.12.0")
set(CLI11_VERSION "2.7.2")
set(RANG_VERSION "3.3")

set(ASIO_URL "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-32-0.tar.gz")
set(ASIO_SHA256 "f1b94b80eeb00bb63a3c8cef5047d4e409df4d8a3fe502305976965827d95672")
set(WEBSOCKETPP_URL "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz")
set(WEBSOCKETPP_SHA256 "6ce889d85ecdc2d8fa07408d6787e7352510750daa66b5ad44aacb47bea76755")
set(JSON_URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp")
set(JSON_SHA256 "aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63")
set(CLI11_URL "https://github.com/CLIUtils/CLI11/releases/download/v2.7.2/CLI11.hpp")
set(CLI11_SHA256 "ffa9a30da295c5858fb5f91f9f45771bab09471d7010a34c7c68c857a330dd76")
set(RANG_URL "https://github.com/agauniyal/rang/archive/refs/tags/v3.3.tar.gz")
set(RANG_SHA256 "e835f79e71e1befb0b0c2906cfaa5cb07e12c6f88789931cd3c86c80c852453a")

function(fetch_dependency name filename url sha256)
  set(archive "${WORK_DIR}/${filename}")
  if(EXISTS "${archive}")
    file(SHA256 "${archive}" existing_sha256)
    if(existing_sha256 STREQUAL sha256)
      message(STATUS "${name}: using cached download")
      set(${name}_ARCHIVE "${archive}" PARENT_SCOPE)
      return()
    endif()
  endif()
  message(STATUS "${name}: downloading")
  file(DOWNLOAD "${url}" "${archive}"
    EXPECTED_HASH "SHA256=${sha256}"
    STATUS download_status
    TLS_VERIFY ON
    SHOW_PROGRESS)
  list(GET download_status 0 download_code)
  list(GET download_status 1 download_message)
  if(NOT download_code EQUAL 0)
    file(REMOVE "${archive}")
    message(FATAL_ERROR "${name}: download failed: ${download_message}")
  endif()
  set(${name}_ARCHIVE "${archive}" PARENT_SCOPE)
endfunction()

function(unpack_dependency name archive)
  set(destination "${WORK_DIR}/src/${name}")
  file(REMOVE_RECURSE "${destination}")
  file(MAKE_DIRECTORY "${destination}")
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${destination}")
  file(GLOB entries "${destination}/*")
  list(LENGTH entries entry_count)
  if(NOT entry_count EQUAL 1)
    message(FATAL_ERROR "${name}: unexpected archive layout")
  endif()
  list(GET entries 0 root)
  set(${name}_ROOT "${root}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${WORK_DIR}")

fetch_dependency(asio "asio.tar.gz" "${ASIO_URL}" "${ASIO_SHA256}")
fetch_dependency(websocketpp "websocketpp.tar.gz" "${WEBSOCKETPP_URL}" "${WEBSOCKETPP_SHA256}")
fetch_dependency(json "json.hpp" "${JSON_URL}" "${JSON_SHA256}")
fetch_dependency(cli11 "CLI11.hpp" "${CLI11_URL}" "${CLI11_SHA256}")
fetch_dependency(rang "rang.tar.gz" "${RANG_URL}" "${RANG_SHA256}")

unpack_dependency(asio "${asio_ARCHIVE}")
unpack_dependency(websocketpp "${websocketpp_ARCHIVE}")
unpack_dependency(rang "${rang_ARCHIVE}")

set(ASIO_INCLUDE "${asio_ROOT}/asio/include")
set(WEBSOCKETPP_INCLUDE "${websocketpp_ROOT}/websocketpp")
set(RANG_HEADER "${rang_ROOT}/include/rang.hpp")

foreach(path
    "${ASIO_INCLUDE}/asio"
    "${ASIO_INCLUDE}/asio.hpp"
    "${WEBSOCKETPP_INCLUDE}"
    "${RANG_HEADER}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "expected path not found: ${path}")
  endif()
endforeach()

file(REMOVE_RECURSE
  "${EXTERN_DIR}/asio"
  "${EXTERN_DIR}/asio.hpp"
  "${EXTERN_DIR}/websocketpp"
  "${EXTERN_DIR}/json")

file(COPY "${ASIO_INCLUDE}/asio" DESTINATION "${EXTERN_DIR}")
file(COPY "${ASIO_INCLUDE}/asio.hpp" DESTINATION "${EXTERN_DIR}")
file(COPY "${WEBSOCKETPP_INCLUDE}" DESTINATION "${EXTERN_DIR}")
file(COPY "${json_ARCHIVE}" DESTINATION "${EXTERN_DIR}/json")
file(COPY "${cli11_ARCHIVE}" DESTINATION "${EXTERN_DIR}")
file(COPY "${RANG_HEADER}" DESTINATION "${EXTERN_DIR}")

message(STATUS "updated asio ${ASIO_VERSION}")
message(STATUS "updated websocketpp ${WEBSOCKETPP_VERSION}")
message(STATUS "updated nlohmann/json ${JSON_VERSION}")
message(STATUS "updated cli11 ${CLI11_VERSION}")
message(STATUS "updated rang ${RANG_VERSION}")
message(STATUS "vendored dependencies written to ${EXTERN_DIR}")
