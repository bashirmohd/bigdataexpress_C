# ----------------------------------------------------------------------

file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} mounts-test*.cc)

foreach(testSrc ${TEST_SRCS})

  get_filename_component(testName ${testSrc} NAME_WE)
  message ("Adding test " ${testName} " to test suite")
  add_executable(${testName} ${testSrc}
    ${PROJECT_SOURCE_DIR}/utils/mounts.cc)
  target_link_libraries(${testName}
    ${BDE_BOOTSTRAPPED_LIB_DIR}/libmount.a
    ${BDE_BOOTSTRAPPED_LIB_DIR}/libblkid.a)

  add_test(NAME ${testName} COMMAND ${testName})

endforeach(testSrc)

# ----------------------------------------------------------------------
