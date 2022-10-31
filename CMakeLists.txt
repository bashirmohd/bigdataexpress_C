cmake_minimum_required(VERSION 3.5)

# VERSION string is of the form "major.minor.patch.tweak".
project(BDE
  VERSION 1.0.0.0)

# ----------------------------------------------------------------------
# Create a compile_commands.json file.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------------------------------------------------
# Use ccache when available

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# ----------------------------------------------------------------------

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "default install path" FORCE)
  endif()

# ----------------------------------------------------------------------

set(CMAKE_CXX_FLAGS "-std=c++11 -DASIO_STANDALONE ${CMAKE_CXX_FLAGS}")

# ----------------------------------------------------------------------
# Find some VCS information.

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

execute_process(
  COMMAND git log -1 --format=%s
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

execute_process(
  COMMAND git log -1 --format=%ae
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_AUTHOR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

execute_process(
  COMMAND git log -1 --format=%ad --date=format:"%b %d %Y %T"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
add_definitions("-DGIT_BRANCH=${GIT_BRANCH}")

configure_file(${PROJECT_SOURCE_DIR}/bde_version.h.in
  ${PROJECT_SOURCE_DIR}/bde_version.h)


# ----------------------------------------------------------------------
# Find C/C++ libraries.

# pthread
find_package (Threads)

find_package (ZLIB REQUIRED)
find_package (OpenSSL REQUIRED)
find_library (LIBRT_LIBRARIES rt)

# include path
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/install/include
    ${PROJECT_SOURCE_DIR}/install/include/bsoncxx/v_noabi
    ${PROJECT_SOURCE_DIR}/install/include/mongocxx/v_noabi
    ${PROJECT_SOURCE_DIR}/server
)

if (EXISTS "/etc/redhat-release")
  set (BDE_BOOTSTRAPPED_LIB_DIR   ${PROJECT_SOURCE_DIR}/install/lib)
  set (BDE_BOOTSTRAPPED_LIB64_DIR ${PROJECT_SOURCE_DIR}/install/lib64)
elseif (EXISTS "/etc/debian_version")
  set (BDE_BOOTSTRAPPED_LIB_DIR   ${PROJECT_SOURCE_DIR}/install/lib)
  set (BDE_BOOTSTRAPPED_LIB64_DIR ${PROJECT_SOURCE_DIR}/install/lib/x86_64-linux-gnu)
endif ()

link_directories(
  ${BDE_BOOTSTRAPPED_LIB_DIR}
  ${BDE_BOOTSTRAPPED_LIB64_DIR}
)

add_subdirectory(server)
add_subdirectory(agent)
add_subdirectory(utils)
add_subdirectory(rpctest)

# ----------------------------------------------------------------------
# Enable/disable building test suite.

option(BDE_ENABLE_TESTING "Enable testing" OFF)

if (BDE_ENABLE_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif ()

# ----------------------------------------------------------------------
# Add a Make rule for generating tags.

# Add target "tags" (with alias "etags")
if (UNIX)
  add_custom_target(tags
    COMMAND find . /usr/include -iname "*.cc" -or -iname "*.c" -or -iname "*.cpp" -or -iname "*.h" -or -iname "*.hpp" | etags --members --declarations -
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
  add_custom_target(etags DEPENDS tags)
endif (UNIX)

# ----------------------------------------------------------------------
# Create rpm/deb packages with CPack.

set(CPACK_PACKAGE_VERSION  ${BDE_VERSION_MAJOR}.${BDE_VERSION_MINOR}.${BDE_VERSION_PATCH})
set(CPACK_PACKAGE_RELEASE  1)

set(CPACK_GENERATOR        "RPM;DEB;TGZ")

set(CPACK_PACKAGE_NAME                 "big-data-express-agent")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  "Agent component of Big Data Express project")
set(CPACK_PACKAGE_CONTACT              "Sajith Sasidharan <sajith@fnal.gov>")
set(CPACK_PACKAGE_VENDOR               "Fermilab")
set(CPACK_PACKAGING_INSTALL_PREFIX     ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME            "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_PACKAGE_DESCRIPTION_FILE     "${PROJECT_SOURCE_DIR}/packaging/Description.txt")
set(CPACK_RPM_CHANGELOG_FILE           "${PROJECT_SOURCE_DIR}/packaging/Changelog.txt")

set(CPACK_RPM_PACKAGE_LICENSE          "Proprietary")
set(CPACK_RPM_PACKAGE_GROUP            "Networking/File transfer")
set(CPACK_RPM_PACKAGE_URL              "http://bigdataexpress.fnal.gov")

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE,  "amd64")
set(CPACK_DEBIAN_PACKAGE_VERSION,       1)
set(CPACK_DEBIAN_PACKAGE_SECTION,       "Network")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE,      "http://bigdataexpress.fnal.gov")

# configure_file("${CMAKE_CURRENT_SOURCE_DIR}/package/rpm/bde-agent.spec.in" "${CMAKE_CURRENT_BINARY_DIR}/bde-agent.spec" @ONLY IMMEDIATE)
# set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/bde-agent.spec")

configure_file(${PROJECT_SOURCE_DIR}/scripts/system/bdeserver.service.in
  ${PROJECT_BINARY_DIR}/scripts/system/bdeserver.service)

configure_file(${PROJECT_SOURCE_DIR}/scripts/system/bdeagent.service.in
  ${PROJECT_BINARY_DIR}/scripts/system/bdeagent.service)

include(CPack)

# ----------------------------------------------------------------------
# Use Sphinx for documentation.

option(BDE_ENABLE_MANUAL "Enable building HTML version of manual" OFF)
option(BDE_ENABLE_PDF_MANUAL "Enable PDF version of manual" OFF)

if (BDE_ENABLE_MANUAL)
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
  find_package(Sphinx REQUIRED)
  add_subdirectory(docs)
endif ()

# ----------------------------------------------------------------------

