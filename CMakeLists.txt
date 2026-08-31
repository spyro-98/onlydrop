cmake_minimum_required(VERSION 3.20)

project(OnlyDrop VERSION 0.1.0 LANGUAGES C)

option(ONLYDROP_ENABLE_SERVER "Build the libevent HTTP server" ON)
option(ONLYDROP_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ONLYDROP_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(PkgConfig REQUIRED)

set(ONLYDROP_HAVE_LIBEVENT 0)
set(ONLYDROP_HAVE_QRENCODE 0)
if(ONLYDROP_ENABLE_SERVER)
  pkg_check_modules(LIBEVENT QUIET IMPORTED_TARGET libevent)
  if(TARGET PkgConfig::LIBEVENT)
    set(ONLYDROP_HAVE_LIBEVENT 1)
  else()
    message(WARNING "libevent was not found: onlydrop will build without HTTP serving support")
  endif()
endif()
pkg_check_modules(QRENCODE QUIET IMPORTED_TARGET libqrencode)
if(TARGET PkgConfig::QRENCODE)
  set(ONLYDROP_HAVE_QRENCODE 1)
else()
  message(WARNING "libqrencode was not found: --qr will be unavailable")
endif()

add_executable(onlydrop
  src/main.c
  src/cli.c
  src/config.c
  src/lifecycle.c
  src/payload.c
  src/protocol_http.c
  src/security.c
  src/platform_posix.c
  src/qr.c
)

target_include_directories(onlydrop PRIVATE include)
target_link_libraries(onlydrop PRIVATE OpenSSL::Crypto)
target_compile_definitions(onlydrop PRIVATE ONLYDROP_HAVE_LIBEVENT=${ONLYDROP_HAVE_LIBEVENT} ONLYDROP_HAVE_QRENCODE=${ONLYDROP_HAVE_QRENCODE})

if(TARGET PkgConfig::LIBEVENT)
  target_link_libraries(onlydrop PRIVATE PkgConfig::LIBEVENT)
endif()
if(TARGET PkgConfig::QRENCODE)
  target_link_libraries(onlydrop PRIVATE PkgConfig::QRENCODE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(onlydrop PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
  if(ONLYDROP_ENABLE_ASAN)
    target_compile_options(onlydrop PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(onlydrop PRIVATE -fsanitize=address)
  endif()
  if(ONLYDROP_ENABLE_UBSAN)
    target_compile_options(onlydrop PRIVATE -fsanitize=undefined)
    target_link_options(onlydrop PRIVATE -fsanitize=undefined)
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS onlydrop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
