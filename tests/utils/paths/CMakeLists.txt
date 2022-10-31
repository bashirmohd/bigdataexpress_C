# ----------------------------------------------------------------------

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *-test*.cc)

foreach(testSrc ${TEST_SRCS})

  get_filename_component(testName ${testSrc} NAME_WE)
  message ("Adding test " ${testName} " to test suite")
  add_executable(${testName} ${testSrc})
  target_link_libraries(${testName} PathGroups)
  add_test(NAME ${testName} COMMAND ${testName})

endforeach(testSrc)

# ----------------------------------------------------------------------
