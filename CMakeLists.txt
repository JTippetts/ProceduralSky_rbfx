cmake_minimum_required (VERSION 3.4.3)

# Name your project. Can be anything.
project (rbfx_test)

# Minimal supported C++ version.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set binary output directories. Changing these paths will require you adjusting your resource path configuration!
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include cloned engine directory in a build.
# add_subdirectory(Urho3D)
include (${URHO3D_SDK}/share/cmake/Modules/UrhoCommon.cmake)
include(${URHO3D_SDK}/share/CMake/Urho3D.cmake)

# Define executable name.
add_executable(rbfx_test WIN32 main.cpp)

# Link to game engine library.
target_link_libraries(rbfx_test Urho3D)