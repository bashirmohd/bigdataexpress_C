# ----------------------------------------------------------------------

add_executable(bdeagent
  bdeagent.cc
  main.cc
  agent.cc
  agentmanager.cc
  localstorageagent.cc
  sharedstorageagent.cc
  dtnagent.cc
  launcheragent.cc
  sitestore.cc)

target_link_libraries(bdeagent
  utils
  PathGroups
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongoc-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libsnappy.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbson-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurlcpp.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurl.a
  ${CMAKE_THREAD_LIBS_INIT}
  ${LIBRT_LIBRARIES}
  ${ZLIB_LIBRARIES})

# ----------------------------------------------------------------------

# Disabling building scannerd for now; uncomment when needed.

# add_executable(scannerd
#   scannerd.cc
#   scanner.cc
#   agent.cc
#   agentmanager.cc
#   localstorageagent.cc
#   sharedstorageagent.cc
#   dtnagent.cc
#   sitestore.cc)

# target_link_libraries(scannerd
#   pthread
#   utils
#   PathGroups
#   bsoncxx
#   mongocxx
#   curlcpp
#   curl)

# ----------------------------------------------------------------------

# These rules will be used when we run "make install" (and thereby
# when running "cpack" to create binary packages.)

add_custom_target(copy-agent-config-files ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/agent/config ${CMAKE_BINARY_DIR}/agent/config
  DEPENDS bdeagent)

install(TARGETS bdeagent DESTINATION bin)
install(FILES README-bdeagent.txt DESTINATION share/doc/bdeagent)
install(FILES config/agent.example.json DESTINATION etc/bdeagent)
install(FILES config/launcher.example.json DESTINATION etc/bdeagent)

# Configure and install a systemd service file for BDE Agent.
configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/system/bdeagent.service.in
  ${PROJECT_BINARY_DIR}/scripts/system/bdeagent.service)

install(FILES
  ${PROJECT_BINARY_DIR}/scripts/system/bdeagent.service
  DESTINATION /lib/systemd/system/)

# Configure and install a systemd service file for BDE Launcher Agent.
configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/system/bdelauncher.service.in
  ${PROJECT_BINARY_DIR}/scripts/system/bdelauncher.service)

install(FILES
  ${PROJECT_BINARY_DIR}/scripts/system/bdelauncher.service
  DESTINATION /lib/systemd/system/)

# ----------------------------------------------------------------------
