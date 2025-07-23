# Project
cmake_minimum_required(VERSION 3.14...3.22 FATAL_ERROR)
project(falaclient LANGUAGES CXX C)

# Include modules
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 14)
message(STATUS "Using cmake ${CMAKE_VERSION}")

# Build type
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Using build type '${DEFAULT_BUILD_TYPE}' (default).")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
else()
  message(STATUS "Using build type '${CMAKE_BUILD_TYPE}'.")
endif()

# Static linking - no runtime paths needed
set(CMAKE_SKIP_RPATH TRUE)

# Options
option(HAS_COREDUMP "Core Dump" ON)
option(FALACLIENT_INSTALL_DEPS "Install system dependencies" OFF)
option(FALACLIENT_FORMAT_CODE "Format source code with uncrustify" OFF)
option(FALACLIENT_BUILD_DOC "Build documentation" OFF)
option(FALACLIENT_BUMP_VERSION "Bump version" OFF)

message(STATUS "Core Dump: ${HAS_COREDUMP}")

# Ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Custom function to detect Linux distribution
function(detect_linux_distro DISTRO_VAR)
  if(EXISTS "/etc/os-release")
    file(READ "/etc/os-release" OS_RELEASE_CONTENT)
    string(REGEX MATCH "NAME=\"?([^\"\\n]+)\"?" NAME_MATCH "${OS_RELEASE_CONTENT}")
    if(NAME_MATCH)
      string(REGEX REPLACE "NAME=\"?([^\"\\n]+)\"?" "\\1" DISTRO_NAME "${NAME_MATCH}")
      set(${DISTRO_VAR} "${DISTRO_NAME}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

# Custom target for installing dependencies
if(FALACLIENT_INSTALL_DEPS)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    detect_linux_distro(LINUX_DISTRO)
    message(STATUS "Detected Linux distribution: ${LINUX_DISTRO}")
    
    if(LINUX_DISTRO MATCHES "Ubuntu|Raspbian GNU/Linux|Debian GNU/Linux|Pop!_OS")
      add_custom_target(install_deps
        COMMAND sudo apt update
        COMMAND sudo apt -y install cmake build-essential libssl-dev libreadline-dev 
                libncurses5-dev libxapian-dev libsqlite3-dev libsasl2-dev libsasl2-modules 
                libcurl4-openssl-dev libexpat-dev zlib1g-dev libmagic-dev uuid-dev
        COMMENT "Installing dependencies for Ubuntu/Debian"
      )
    elseif(LINUX_DISTRO MATCHES "Fedora|Fedora Linux|Rocky Linux")
      add_custom_target(install_deps
        COMMAND sudo yum -y install cmake openssl-devel ncurses-devel xapian-core-devel 
                sqlite-devel cyrus-sasl-devel cyrus-sasl-plain libcurl-devel expat-devel 
                zlib-devel file-devel libuuid-devel clang
        COMMENT "Installing dependencies for Fedora/Rocky Linux"
      )
    elseif(LINUX_DISTRO MATCHES "Arch Linux")
      add_custom_target(install_deps
        COMMAND sudo pacman --needed -Sy cmake make openssl ncurses xapian-core sqlite 
                cyrus-sasl curl expat zlib file
        COMMENT "Installing dependencies for Arch Linux"
      )
    elseif(LINUX_DISTRO MATCHES "Gentoo")
      add_custom_target(install_deps
        COMMAND sudo emerge -n dev-util/cmake dev-libs/openssl sys-libs/ncurses 
                dev-libs/xapian dev-db/sqlite dev-libs/cyrus-sasl net-misc/curl 
                dev-libs/expat sys-libs/zlib sys-apps/file
        COMMENT "Installing dependencies for Gentoo"
      )
    elseif(LINUX_DISTRO MATCHES "Alpine Linux")
      add_custom_target(install_deps
        COMMAND sudo apk add git build-base cmake ncurses-dev openssl-dev xapian-core-dev 
                sqlite-dev curl-dev expat-dev cyrus-sasl-dev cyrus-sasl-login file-dev 
                util-linux-dev zlib-dev linux-headers
        COMMENT "Installing dependencies for Alpine Linux"
      )
    elseif(LINUX_DISTRO MATCHES "openSUSE Tumbleweed")
      add_custom_target(install_deps
        COMMAND sudo zypper install -y -t pattern devel_C_C++
        COMMAND sudo zypper install -y cmake libopenssl-devel libxapian-devel sqlite3-devel 
                libcurl-devel libexpat-devel file-devel
        COMMENT "Installing dependencies for openSUSE Tumbleweed"
      )
    elseif(LINUX_DISTRO MATCHES "Void")
      add_custom_target(install_deps
        COMMAND sudo xbps-install -y base-devel ccache cmake openssl-devel xapian-core-devel 
                sqlite-devel libcurl-devel expat-devel libsasl-devel cyrus-sasl-modules file-devel
        COMMENT "Installing dependencies for Void Linux"
      )
    else()
      message(WARNING "Unsupported Linux distribution: ${LINUX_DISTRO}")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_program(BREW_PROGRAM brew)
    find_program(PORT_PROGRAM port)
    
    if(BREW_PROGRAM)
      add_custom_target(install_deps
        COMMAND ${CMAKE_COMMAND} -E env HOMEBREW_NO_AUTO_UPDATE=1 ${BREW_PROGRAM} install 
                openssl ncurses xapian sqlite libmagic ossp-uuid
        COMMENT "Installing dependencies with Homebrew"
      )
    elseif(PORT_PROGRAM)
      add_custom_target(install_deps
        COMMAND sudo ${PORT_PROGRAM} -N install openssl ncurses xapian-core sqlite3 libmagic ossp-uuid
        COMMENT "Installing dependencies with MacPorts"
      )
    else()
      message(FATAL_ERROR "Neither Homebrew nor MacPorts found on macOS")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    add_custom_target(install_deps
      COMMAND doas pkg_add cmake curl sqlite3 xapian-core libmagic curl expat cyrus-sasl ossp-uuid libiconv
      COMMENT "Installing dependencies for OpenBSD"
    )
  else()
    message(FATAL_ERROR "Unsupported operating system: ${CMAKE_SYSTEM_NAME}")
  endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find other libraries
find_library(MAGIC_LIBRARY NAMES magic REQUIRED)
find_path(MAGIC_INCLUDE_DIR magic.h REQUIRED)

# UUID library handling - different on OpenBSD
if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  find_library(UUID_LIBRARY NAMES ossp-uuid uuid PATHS /usr/local/lib REQUIRED)
  find_path(UUID_INCLUDE_DIR uuid.h PATHS /usr/local/include REQUIRED)
else()
  find_library(UUID_LIBRARY NAMES uuid REQUIRED)
  find_path(UUID_INCLUDE_DIR uuid/uuid.h REQUIRED)
endif()

find_library(XAPIAN_LIBRARY NAMES xapian REQUIRED)
find_path(XAPIAN_INCLUDE_DIR xapian.h REQUIRED)

find_library(SQLITE_LIBRARY NAMES sqlite3 REQUIRED)
find_path(SQLITE_INCLUDE_DIR sqlite3.h REQUIRED)

find_library(CURL_LIBRARY NAMES curl REQUIRED)
find_path(CURL_INCLUDE_DIR curl/curl.h REQUIRED)

find_library(EXPAT_LIBRARY NAMES expat REQUIRED)
find_path(EXPAT_INCLUDE_DIR expat.h REQUIRED)

find_library(SASL_LIBRARY NAMES sasl2 REQUIRED)
find_path(SASL_INCLUDE_DIR sasl/sasl.h REQUIRED)

find_library(Z_LIBRARY NAMES z REQUIRED)
find_path(Z_INCLUDE_DIR zlib.h REQUIRED)

message(STATUS "Found libmagic: ${MAGIC_LIBRARY} and ${MAGIC_INCLUDE_DIR}")
message(STATUS "Found libuuid: ${UUID_LIBRARY} and ${UUID_INCLUDE_DIR}")

# Source code formatting target
if(FALACLIENT_FORMAT_CODE)
  find_program(UNCRUSTIFY_PROGRAM uncrustify)
  if(UNCRUSTIFY_PROGRAM)
    file(GLOB_RECURSE ALL_SOURCE_FILES
      "${CMAKE_SOURCE_DIR}/src/*.cpp"
      "${CMAKE_SOURCE_DIR}/src/*.h"
    )
    
    add_custom_target(format_code
      COMMAND ${UNCRUSTIFY_PROGRAM} -c ${CMAKE_SOURCE_DIR}/etc/uncrustify.cfg --replace --no-backup ${ALL_SOURCE_FILES}
      COMMENT "Formatting source code with uncrustify"
    )
  else()
    message(WARNING "uncrustify not found, cannot format code")
  endif()
endif()

# Version bump target
if(FALACLIENT_BUMP_VERSION)
  add_custom_target(bump_version
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/BumpVersion.cmake
    COMMENT "Bumping version"
  )
endif()

# Documentation target  
if(FALACLIENT_BUILD_DOC)
  find_program(HELP2MAN_PROGRAM help2man)
  if(HELP2MAN_PROGRAM)
    add_custom_target(build_docs
      COMMAND ${HELP2MAN_PROGRAM} -n "ncurses mail" -N -o ${CMAKE_SOURCE_DIR}/src/falaclient.1 $<TARGET_FILE:falaclient>
      DEPENDS falaclient
      COMMENT "Generating manual page"
    )
  else()
    message(WARNING "help2man not found, cannot generate documentation")
  endif()
endif()

# Application
add_executable(falaclient
  ext/apathy/path.hpp
  ext/cxx-prettyprint/prettyprint.hpp
  ext/cyrus-imap/lib/imapurl.c
  ext/cyrus-imap/lib/imapurl.h
  ext/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h
  src/addressbook.cpp
  src/addressbook.h
  src/auth.cpp
  src/auth.h
  src/body.cpp
  src/body.h
  src/cacheutil.cpp
  src/cacheutil.h
  src/config.cpp
  src/config.h
  src/contact.cpp
  src/contact.h
  src/crypto.cpp
  src/crypto.h
  src/encoding.cpp
  src/encoding.h
  src/flag.cpp
  src/flag.h
  src/header.cpp
  src/header.h
  src/html_parser.cpp
  src/html_parser.h
  src/imap.cpp
  src/imap.h
  src/imapcache.cpp
  src/imapcache.h
  src/imapindex.cpp
  src/imapindex.h
  src/imapmanager.cpp
  src/imapmanager.h
  src/libetpan_help.h
  src/lockfile.cpp
  src/lockfile.h
  src/log.cpp
  src/log.h
  src/loghelp.cpp
  src/loghelp.h
  src/main.cpp
  src/maphelp.h
  src/offlinequeue.cpp
  src/offlinequeue.h
  src/sasl.cpp
  src/sasl.h
  src/searchengine.cpp
  src/searchengine.h
  src/serialization.h
  src/sethelp.h
  src/sleepdetect.cpp
  src/sleepdetect.h
  src/smtp.cpp
  src/smtp.h
  src/smtpmanager.cpp
  src/smtpmanager.h
  src/sqlitehelp.cpp
  src/sqlitehelp.h
  src/status.cpp
  src/status.h
  src/ui.cpp
  src/ui.h
  src/util.cpp
  src/util.h
  src/version.cpp
  src/version.h
)

# libetpan configuration
option(LIBETPAN_CUSTOM "Use custom libetpan" ON)
message(STATUS "Custom libetpan: ${LIBETPAN_CUSTOM}")

if(LIBETPAN_CUSTOM)
  set(LIBETPAN_LIBNAME "etpan-falaclient")
  message(STATUS "Libetpan custom lib name: ${LIBETPAN_LIBNAME}")
  message(STATUS "Libetpan: static")
  message(STATUS "Libetpan: skip install")
  
  # Configure libetpan as static library and skip install
  set(LIBETPAN_STATIC ON)
  set(LIBETPAN_NOINSTALL ON)
  
  # Add libetpan as subdirectory
  add_subdirectory(ext/libetpan)
  add_dependencies(falaclient etpan-falaclient)
  target_compile_definitions(falaclient PRIVATE LIBETPAN_CUSTOM=1)
  
  set(LIBETPAN_LIBRARY etpan-falaclient)
  set(LIBETPAN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ext/libetpan/include")
  # Also include the build directory for generated headers
  target_include_directories(falaclient PRIVATE "${CMAKE_BINARY_DIR}/ext/libetpan/include")
else()
  find_library(LIBETPAN_LIBRARY NAMES etpan REQUIRED)
  find_path(LIBETPAN_INCLUDE_DIR libetpan/libetpan.h REQUIRED)
endif()

message(STATUS "Found libetpan: ${LIBETPAN_LIBNAME}")

# Install target
install(TARGETS falaclient DESTINATION bin)

# Include directories
target_include_directories(falaclient PRIVATE "ext")
target_include_directories(falaclient PRIVATE "ext/sqlite_modern_cpp/hdr")
target_include_directories(falaclient PRIVATE "ext/cyrus-imap/lib")
target_include_directories(falaclient PRIVATE "ext/cereal/include")
target_include_directories(falaclient PRIVATE ${LIBETPAN_INCLUDE_DIR} ${XAPIAN_INCLUDE_DIR}
  ${MAGIC_INCLUDE_DIR} ${UUID_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${EXPAT_INCLUDE_DIR}
  ${SASL_INCLUDE_DIR} ${Z_INCLUDE_DIR} ${SQLITE_INCLUDE_DIR})

# Compiler flags
set_target_properties(falaclient PROPERTIES COMPILE_FLAGS
  "-Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -Wno-missing-braces -Wswitch-default -Wcast-align -Wunreachable-code -Wundef -Wuninitialized")

# Compiler definitions
if(HAS_COREDUMP)
  target_compile_definitions(falaclient PRIVATE HAS_COREDUMP="${HAS_COREDUMP}")
endif()

# macOS specific
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/falaclient.entitlements")
    set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/src/falaclient.entitlements")
    set(SIGNSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/util/sign")
    install(CODE "execute_process(COMMAND
      \"${SIGNSCRIPT}\" \"${ENTITLEMENTS}\" \"${CMAKE_INSTALL_PREFIX}/bin/falaclient\"
      )")
  endif()
endif()

# NCurses
target_compile_options(falaclient PUBLIC ${CURSES_CFLAGS})

# Check for execinfo.h
check_include_file(execinfo.h HAVE_EXECINFO_H)
if(HAVE_EXECINFO_H)
  target_compile_definitions(falaclient PRIVATE HAVE_EXECINFO_H=1)
endif()

# Library linking
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_library(ICONV_LIBRARY iconv)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(SECURITY_LIBRARY Security)
  
  if(ICONV_LIBRARY AND COREFOUNDATION_LIBRARY AND SECURITY_LIBRARY)
    target_link_libraries(falaclient PUBLIC ${Z_LIBRARY} "${ICONV_LIBRARY}" "${COREFOUNDATION_LIBRARY}" "${SECURITY_LIBRARY}")
  endif()
  
  find_library(CFNETWORK_LIBRARY CFNetwork)
  if(CFNETWORK_LIBRARY)
    target_link_libraries(falaclient PUBLIC "${CFNETWORK_LIBRARY}")
  endif()
endif()

target_link_libraries(falaclient PUBLIC
  ${LIBETPAN_LIBRARY}
  ${XAPIAN_LIBRARY}
  ${MAGIC_LIBRARY}
  ${UUID_LIBRARY}
  ${CURL_LIBRARY}
  ${EXPAT_LIBRARY}
  ${SASL_LIBRARY}
  ${Z_LIBRARY}
  ${SQLITE_LIBRARY}
  ${CURSES_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  Threads::Threads
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(falaclient PUBLIC -rdynamic)
endif()

# Install man page
install(FILES src/falaclient.1 DESTINATION share/man/man1)

# Utils
configure_file(src/oauth2falaclient ${CMAKE_CURRENT_BINARY_DIR}/oauth2falaclient COPYONLY)
configure_file(src/html2falaclient ${CMAKE_CURRENT_BINARY_DIR}/html2falaclient COPYONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/oauth2falaclient DESTINATION bin)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/html2falaclient DESTINATION bin)

# Install share files
install(DIRECTORY themes/ DESTINATION share/falaclient/themes)
install(FILES etc/main.conf DESTINATION share/falaclient)

# Tests (placeholder)
enable_testing()
add_test(NAME falaclient_version_test COMMAND falaclient --version)

# Add custom targets for convenience
add_custom_target(deps
  COMMENT "Install dependencies (use -DFALACLIENT_INSTALL_DEPS=ON)"
)

add_custom_target(format
  COMMENT "Format source code (use -DFALACLIENT_FORMAT_CODE=ON)"
)

add_custom_target(docs
  COMMENT "Build documentation (use -DFALACLIENT_BUILD_DOC=ON)"
)

add_custom_target(version_bump
  COMMENT "Bump version (use -DFALACLIENT_BUMP_VERSION=ON)"
)

# Print usage information
add_custom_target(help_usage
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "Falaclient CMake Build System"
  COMMAND ${CMAKE_COMMAND} -E echo "=============================="
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "Basic build:"
  COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
  COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
  COMMAND ${CMAKE_COMMAND} -E echo "  make -j$(nproc)"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "Options:"
  COMMAND ${CMAKE_COMMAND} -E echo "  -DFALACLIENT_INSTALL_DEPS=ON     Install system dependencies"
  COMMAND ${CMAKE_COMMAND} -E echo "  -DFALACLIENT_FORMAT_CODE=ON      Format source code"
  COMMAND ${CMAKE_COMMAND} -E echo "  -DFALACLIENT_BUILD_DOC=ON        Build documentation"
  COMMAND ${CMAKE_COMMAND} -E echo "  -DFALACLIENT_BUMP_VERSION=ON     Bump version"
  COMMAND ${CMAKE_COMMAND} -E echo "  -DCMAKE_BUILD_TYPE=Debug         Debug build"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "Examples:"
  COMMAND ${CMAKE_COMMAND} -E echo "  cmake -DFALACLIENT_INSTALL_DEPS=ON .."
  COMMAND ${CMAKE_COMMAND} -E echo "  cmake -DCMAKE_BUILD_TYPE=Debug .."
  COMMAND ${CMAKE_COMMAND} -E echo ""
)
