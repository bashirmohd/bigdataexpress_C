add_executable(bdeserver
    bdeserver.cc
    main.cc
    sitestore.cc
    scheduler.cc
    resourcegraph.cc
    resourcemanager.cc
    jobmanager.cc
    #portal.cc
    wan.cc
    )

target_link_libraries(bdeserver
  utils
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongoc-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libsnappy.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbson-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurlcpp.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurl.a
  ${CMAKE_THREAD_LIBS_INIT}
  ${LIBRT_LIBRARIES}
  ${ZLIB_LIBRARIES}
  )

add_custom_target(copy-bde-config-files ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/server/config ${CMAKE_BINARY_DIR}/server/config
  DEPENDS bdeserver)

add_executable(bde_clear_db bde_clear_db.cc sitestore.cc)

target_link_libraries(bde_clear_db
  utils
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongoc-1.0.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libsnappy.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbson-1.0.a
  ${LIBRT_LIBRARIES}
  ${ZLIB_LIBRARIES}
  )

#add_executable(bdeserver bdeserver.cc asiohandler.cpp)
#target_link_libraries(bdeserver amqpcpp ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

# create install path
install(DIRECTORY DESTINATION /var/lib/bdeserver/log)
install(DIRECTORY DESTINATION /var/lib/bdeserver/run)

# executable
install(TARGETS bdeserver RUNTIME DESTINATION bin)
install(TARGETS bde_clear_db RUNTIME DESTINATION bin)

# init script for systemd
configure_file(
    ${PROJECT_SOURCE_DIR}/scripts/system/bdeserver.service.in
    ${PROJECT_BINARY_DIR}/scripts/system/bdeserver.service )

# systemd
install(FILES ${PROJECT_BINARY_DIR}/scripts/system/bdeserver.service DESTINATION /lib/systemd/system/)
install(CODE "execute_process(COMMAND systemctl enable bdeserver.service)")

# config files
install(DIRECTORY config DESTINATION etc/bdeserver)



