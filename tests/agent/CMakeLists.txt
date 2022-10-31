add_executable(dtnagent-test
  dtnagent-test.cc
  ${PROJECT_SOURCE_DIR}/agent/agent.cc
  ${PROJECT_SOURCE_DIR}/agent/agentmanager.cc
  ${PROJECT_SOURCE_DIR}/agent/dtnagent.cc
  ${PROJECT_SOURCE_DIR}/agent/localstorageagent.cc
  ${PROJECT_SOURCE_DIR}/agent/sharedstorageagent.cc
  ${PROJECT_SOURCE_DIR}/agent/launcheragent.cc
  ${PROJECT_SOURCE_DIR}/agent/sitestore.cc)

target_link_libraries(dtnagent-test
  utils
  PathGroups
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongoc-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbson-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libsnappy.a
  ${LIBRT_LIBRARIES}
  ${ZLIB_LIBRARIES})

add_test(NAME dtnagent-test
  COMMAND dtnagent-test
  -c ${PROJECT_SOURCE_DIR}/tests/agent/dtnagent.sample.conf)

# ----------------------------------------------------------------------

add_executable(dtnagent-agent-test
  dtnagent-agent-test.cc
  ${PROJECT_SOURCE_DIR}/agent/main.cc
  ${PROJECT_SOURCE_DIR}/agent/agent.cc
  ${PROJECT_SOURCE_DIR}/agent/agentmanager.cc
  ${PROJECT_SOURCE_DIR}/agent/localstorageagent.cc
  ${PROJECT_SOURCE_DIR}/agent/sharedstorageagent.cc
  ${PROJECT_SOURCE_DIR}/agent/dtnagent.cc
  ${PROJECT_SOURCE_DIR}/agent/launcheragent.cc
  ${PROJECT_SOURCE_DIR}/agent/sitestore.cc)

target_link_libraries(dtnagent-agent-test
  utils
  PathGroups
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongoc-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbson-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libsnappy.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurlcpp.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurl.a
  ${LIBRT_LIBRARIES}
  ${ZLIB_LIBRARIES})

add_test(NAME dtnagent-agent-test
  COMMAND dtnagent-agent-test
  -c ${PROJECT_SOURCE_DIR}/tests/agent/dtnagent.sample.conf)

add_custom_target(copy-agent-test-config-file ALL
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_SOURCE_DIR}/tests/agent/dtnagent.sample.conf
  ${CMAKE_BINARY_DIR}/tests/agent/dtnagent.sample.conf
  DEPENDS dtnagent-agent-test)

# ----------------------------------------------------------------------
