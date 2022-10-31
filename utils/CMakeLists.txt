add_library(utils STATIC
  utils.cc
  jsoncpp.cc
  mqtt.cc
  rpcserver.cc
  rpcclient.cc
  portal.cc
  mounts.cc
  network.cc
  vlan.cc
  process.cc
  tsdb.cc
  proxy-cert.cc
  proxy-cert-impl.cc
  grid-mapfile.cc
  checksum.cc
  fsusage.cc)

target_link_libraries(utils
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmosquitto.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libbsoncxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmongocxx.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libmount.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libblkid.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurlcpp.a
  ${BDE_BOOTSTRAPPED_LIB_DIR}/libcurl.a
  ${ZLIB_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(paths)
