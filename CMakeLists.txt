cmake_minimum_required(VERSION 3.14)
project(Dain VERSION 0.1.0 LANGUAGES CXX)

# Create interface (header-only) library
add_library(Dain INTERFACE
        include/Dain/dain.h)
add_library(Dain::Dain ALIAS Dain)

target_include_directories(Dain
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(Dain INTERFACE cxx_std_17)

# Install headers + export target
include(GNUInstallDirs)

install(
        DIRECTORY   include/Dain
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
        TARGETS     Dain
        EXPORT      DainTargets
        INCLUDES    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
        EXPORT      DainTargets
        FILE        DainTargets.cmake
        NAMESPACE   Dain::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dain
)

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/DainConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/DainConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dain
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/DainConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/DainConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/DainConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Dain
)