cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_ssl)

add_custom_target(version
  ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/main/version.h.in
                   -D DST=${CMAKE_SOURCE_DIR}/main/version.h
                   -P ${CMAKE_SOURCE_DIR}/cmake/GenerateGitDescribeVersion.cmake
  )
