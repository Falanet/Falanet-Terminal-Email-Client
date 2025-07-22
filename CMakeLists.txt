# Project
cmake_minimum_required(VERSION 3.14...3.22 FATAL_ERROR)
project(falanet LANGUAGES CXX C)
include(CheckIncludeFile)
set(CMAKE_CXX_STANDARD 14)
message(STATUS "Using cmake ${CMAKE_VERSION}")

# Modules
include(CheckIncludeFiles)

# Ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Build type
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Using build type '${DEFAULT_BUILD_TYPE}' (default).")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}")
else()
  message(STATUS "Using build type '${CMAKE_BUILD_TYPE}'.")
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Feature - Core dump
option(HAS_COREDUMP "Core Dump" ON)
message(STATUS "Core Dump: ${HAS_COREDUMP}")

# Application
add_executable(falanet
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
  src/lockfile.cpp
  src/lockfile.h
  src/log.cpp
  src/log.h
  src/loghelp.cpp
  src/loghelp.h
  src/main.cpp
  src/offlinequeue.cpp
  src/offlinequeue.h
  src/sasl.cpp
  src/sasl.h
  src/searchengine.cpp
  src/searchengine.h
  src/serialization.h
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
install(TARGETS falanet DESTINATION bin)

# Platform specifics
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_compile_definitions(_XOPEN_SOURCE_EXTENDED)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/ncurses)
  list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/ncurses)
  list(APPEND OPENSSL_ROOT_DIR /usr/local/opt/openssl)
  list(APPEND OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl)
endif()

# Headers
target_include_directories(falanet PRIVATE "ext")

# Compiler flags
set_target_properties(falanet PROPERTIES COMPILE_FLAGS
                      "-Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith \
                       -Wcast-qual -Wno-missing-braces -Wswitch-default \
                       -Wunreachable-code -Wuninitialized -Wcast-align")
# todo: add -Wundef

# Features
if(HAS_COREDUMP)
  target_compile_definitions(falanet PRIVATE HAS_COREDUMP="${HAS_COREDUMP}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # Core dump entitlements
    set(SIGNSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/util/sign")
    set(ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/src/falanet.entitlements")
    install(CODE "execute_process(COMMAND
      \"${SIGNSCRIPT}\" \"${ENTITLEMENTS}\" \"${CMAKE_INSTALL_PREFIX}/bin/falanet\"
      )" COMPONENT Runtime)
  endif()
endif()

# Dependency ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})
target_compile_options(falanet PUBLIC ${NCURSES_CFLAGS})

# Dependency openssl
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Dependency xapian
find_package(Xapian REQUIRED)

# Dependency sqlite3
find_package(SQLite3 REQUIRED)

# Dependency libetpan
option(HAS_CUSTOM_LIBETPAN "Custom libetpan" ON)
message(STATUS "Custom libetpan: ${HAS_CUSTOM_LIBETPAN}")
if(HAS_CUSTOM_LIBETPAN)
  set(LIBETPAN_STATIC ON)
  set(LIBETPAN_NOINSTALL ON)
  set(LIBETPAN_LIBNAME "etpan-falanet")
  add_subdirectory(ext/libetpan)
  add_dependencies(falanet etpan-falanet)
  target_compile_definitions(falanet PRIVATE LIBETPAN_CUSTOM=1)
  set(LIBETPAN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/ext/libetpan/include)
  set(LIBETPAN_LIBRARY etpan-falanet)
else()
  find_path(LIBETPAN_INCLUDE_DIR
    NAMES libetpan/libetpan.h
    PATHS ${additional_includes}
  )
  find_library(LIBETPAN_LIBRARY
    NAMES etpan
    PATHS ${additional_lib_searchpath}
  )
endif()

if(NOT LIBETPAN_INCLUDE_DIR OR NOT LIBETPAN_LIBRARY)
  message(FATAL_ERROR "ERROR: Could not find libetpan")
else()
  message(STATUS "Found libetpan: ${LIBETPAN_LIBRARY}")
endif()

# Dependency sasl2
find_library(CYRUS_SASL_LIBRARY sasl2)
find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args(sasl2 DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)

# Dependency execinfo
CHECK_INCLUDE_FILE(execinfo.h FOUND_EXECINFO)
if(FOUND_EXECINFO)
  target_compile_definitions(falanet PRIVATE HAVE_EXECINFO_H=1)
endif()

# Dependency magic
find_library(MAGIC_LIBRARY magic)
find_path(MAGIC_HEADERS magic.h)
if(NOT MAGIC_LIBRARY OR NOT MAGIC_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libmagic")
else()
  message(STATUS "Found libmagic: ${MAGIC_LIBRARY} and ${MAGIC_HEADERS}")
endif()

# Dependency libuuid
find_library(LIBUUID_LIBRARIES NAMES uuid)
find_path(LIBUUID_HEADERS uuid.h PATH_SUFFIXES uuid/)
if(NOT LIBUUID_LIBRARIES OR NOT LIBUUID_HEADERS)
  message(FATAL_ERROR "ERROR: Could not find libuuid")
else()
  message(STATUS "Found libuuid: ${LIBUUID_LIBRARIES} and ${LIBUUID_HEADERS}")
endif()

# Dependency platform specifics
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  if (NOT HAS_CUSTOM_LIBETPAN)
    find_package(ZLIB REQUIRED)
    find_library(ICONV_LIBRARY iconv REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
    target_link_libraries(falanet PUBLIC ${ZLIB_LIBRARIES} "${ICONV_LIBRARY}" "${COREFOUNDATION_LIBRARY}" "${SECURITY_LIBRARY}")

    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
    check_include_files("CFNetwork/CFNetwork.h" HAVE_CFNETWORK LANGUAGE C)
    if (HAVE_CFNETWORK)
      find_library(CFNETWORK_LIBRARY CFNetwork REQUIRED)
      target_link_libraries(falanet PUBLIC "${CFNETWORK_LIBRARY}")
    endif()
  endif()
endif()

# Includes
target_include_directories(falanet PRIVATE ${LIBETPAN_INCLUDE_DIR} ${XAPIAN_INCLUDE_DIR}
                           ${MAGIC_HEADERS} ${CYRUS_SASL_INCLUDE_DIR}
                           "ext/sqlite_modern_cpp/hdr" "ext/cereal/include" ${LIBUUID_HEADERS}
                           "ext/cyrus-imap/lib")

# Linking
target_link_libraries(falanet PUBLIC
                      ${CURSES_LIBRARIES} OpenSSL::SSL SQLite::SQLite3
                      ${XAPIAN_LIBRARIES} ${LIBETPAN_LIBRARY} ${CYRUS_SASL_LIBRARY}
                      ${MAGIC_LIBRARY} ${LIBUUID_LIBRARIES}
                      pthread ${CMAKE_DL_LIBS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(falanet PUBLIC -rdynamic)
endif()

# Manual
install(FILES src/falanet.1 DESTINATION share/man/man1)

# Utils
configure_file(src/oauth2falanet ${CMAKE_CURRENT_BINARY_DIR}/oauth2falanet COPYONLY)
configure_file(src/html2falanet ${CMAKE_CURRENT_BINARY_DIR}/html2falanet COPYONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/oauth2falanet DESTINATION bin)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/html2falanet DESTINATION bin)

# Themes
macro(add_theme themename)
  configure_file(themes/${themename} ${CMAKE_CURRENT_BINARY_DIR}/share/falanet/themes/${themename} COPYONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/share/falanet/themes/${themename} DESTINATION share/falanet/themes)
endmacro()
add_theme("default.conf")

# Configuration template
configure_file(etc/main.conf ${CMAKE_CURRENT_BINARY_DIR}/share/falanet/main.conf COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/share/falanet/main.conf DESTINATION share/falanet)

# Install main.conf to user's config directory if it doesn't exist
install(CODE "
  # Get the actual user's home directory (handles sudo case)
  set(ACTUAL_USER_HOME \"\")
  if(DEFINED ENV{SUDO_USER})
    # Running with sudo, get the original user's home
    execute_process(COMMAND getent passwd \"$ENV{SUDO_USER}\"
                    OUTPUT_VARIABLE USER_INFO
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(USER_INFO)
      string(REPLACE \":\" \";\" USER_INFO_LIST \"\${USER_INFO}\")
      list(GET USER_INFO_LIST 5 ACTUAL_USER_HOME)
    endif()
  endif()
  
  # Fallback to current HOME if not running with sudo or if detection failed
  if(NOT ACTUAL_USER_HOME OR ACTUAL_USER_HOME STREQUAL \"\")
    set(ACTUAL_USER_HOME \"$ENV{HOME}\")
  endif()
  
  set(USER_CONFIG_DIR \"\${ACTUAL_USER_HOME}/.config/falanet\")
  set(USER_CONFIG_FILE \"\${ACTUAL_USER_HOME}/.config/falanet/main.conf\")
  
  message(STATUS \"Target user home: \${ACTUAL_USER_HOME}\")
  
  # Create config directory if it doesn't exist
  if(NOT EXISTS \"\${USER_CONFIG_DIR}\")
    file(MAKE_DIRECTORY \"\${USER_CONFIG_DIR}\")
    message(STATUS \"Created config directory: \${USER_CONFIG_DIR}\")
  endif()
  
  # Copy main.conf to user config if it doesn't exist
  if(NOT EXISTS \"\${USER_CONFIG_FILE}\")
    file(COPY \"${CMAKE_CURRENT_BINARY_DIR}/share/falanet/main.conf\" DESTINATION \"\${USER_CONFIG_DIR}\")
    message(STATUS \"Installed default config: \${USER_CONFIG_FILE}\")
    
    # Set proper ownership if we're running as root
    if(DEFINED ENV{SUDO_USER} AND DEFINED ENV{SUDO_UID} AND DEFINED ENV{SUDO_GID})
      execute_process(COMMAND chown -R \"$ENV{SUDO_UID}:$ENV{SUDO_GID}\" \"\${USER_CONFIG_DIR}\"
                      ERROR_QUIET)
      message(STATUS \"Set ownership to user: $ENV{SUDO_USER}\")
    endif()
  else()
    message(STATUS \"Config file already exists: \${USER_CONFIG_FILE}\")
  endif()
")

# Uninstall
add_custom_target(uninstall
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/falanet"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/share/man/man1/falanet.1"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/oauth2falanet"
  COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/html2falanet"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/share/falanet"
)
