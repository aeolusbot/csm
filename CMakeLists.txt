cmake_minimum_required(VERSION 3.5)

project(csm VERSION 2.0.0)

include(GNUInstallDirs)
# GNU install dirs do not define CMAKE_INSTALL_CMAKEDIR directory
set(
  CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  CACHE INTERNAL "CMake configuration files (LIBDIR/cmake/PROJECT_NAME)"
)

add_subdirectory(src)

#------------------------------------------------------------------------------#
#                     Installation rules and CMake config                      #
#------------------------------------------------------------------------------#

install(
  EXPORT csm-targets
  FILE csm-targets.cmake
  DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/src/cmake/csm-config.cmake.in"
  "${CMAKE_BINARY_DIR}/csm-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/csm-config-version.cmake"
  COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    "${CMAKE_BINARY_DIR}/csm-config.cmake"
    "${CMAKE_BINARY_DIR}/csm-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)

