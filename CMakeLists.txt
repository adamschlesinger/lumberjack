cmake_minimum_required(VERSION 3.15)
project(lumberjack VERSION 1.0.0 LANGUAGES CXX)

# Build options
option(lumberjack_BUILD_EXAMPLES "Build example programs" OFF)
option(lumberjack_BUILD_TESTS "Build test suite" OFF)

# Library target
add_library(lumberjack STATIC
    src/core.cpp
    src/builtin.cpp
)

# Create alias for namespaced target
add_library(lumberjack::lumberjack ALIAS lumberjack)

# Set C++17 standard requirement
target_compile_features(lumberjack PUBLIC cxx_std_17)

# Include directories
target_include_directories(lumberjack
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Conditionally build examples
if(lumberjack_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Conditionally build tests
if(lumberjack_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS lumberjack
    EXPORT lumberjackTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
)

# Install CMake config files
install(EXPORT lumberjackTargets
    FILE lumberjackTargets.cmake
    NAMESPACE lumberjack::
    DESTINATION lib/cmake/lumberjack
)

# Generate and install config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lumberjackConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/lumberjackConfig.cmake
    INSTALL_DESTINATION lib/cmake/lumberjack
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/lumberjackConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lumberjackConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lumberjackConfigVersion.cmake
    DESTINATION lib/cmake/lumberjack
)
