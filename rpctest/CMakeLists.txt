add_executable(rpctest rpctest.cc)
target_link_libraries(rpctest utils)

add_executable(rpcserver rpcserver.cc)
target_link_libraries(rpcserver utils)

add_custom_target(copy-rpctest-param-files ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/rpctest/params ${CMAKE_BINARY_DIR}/rpctest/params
  DEPENDS rpctest)

add_executable(portaltest portaltest.cc ${CMAKE_SOURCE_DIR}/server/wan.cc)
target_link_libraries(portaltest utils)
