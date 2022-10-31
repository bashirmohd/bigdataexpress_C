# ----------------------------------------------------------------------

add_subdirectory(paths)
add_subdirectory(mounts)

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *-test*.cc)

foreach(testSrc ${TEST_SRCS})

  get_filename_component(testName ${testSrc} NAME_WE)
  message ("Adding test " ${testName} " to test suite")

  add_executable(${testName} ${testSrc})
  target_link_libraries(${testName}
    utils
    PathGroups
    ${ZLIB_LIBRARIES})
  add_test(NAME ${testName} COMMAND ${testName})

endforeach(testSrc)

file(COPY
  ${CMAKE_CURRENT_SOURCE_DIR}/grid-mapfile-read
  DESTINATION
  ${PROJECT_BINARY_DIR}/tests/utils)

# ----------------------------------------------------------------------

add_executable(daemon daemon.cc)
add_executable(nodaemon nodaemon.cc)

# ----------------------------------------------------------------------
