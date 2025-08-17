cmake_minimum_required(VERSION 3.16)

project(ServerApp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Set OpenCV path and find OpenCV using its CMake files
set(OpenCV_DIR "../opencv/build")
find_package(OpenCV REQUIRED 
    PATHS "${OpenCV_DIR}"
    NO_DEFAULT_PATH
)

# Include directories
include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    server.cpp
    src/ProcessManager.cpp
    src/webcam.cpp
    src/captureScreen.cpp
)

# Create executable
add_executable(server ${SOURCES})

# Link libraries
target_link_libraries(server 
    PRIVATE
    Threads::Threads
    ${OpenCV_LIBS}
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(server 
        PRIVATE
        ws2_32
        advapi32
        user32
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(server PRIVATE /W3)
else()
    target_compile_options(server PRIVATE -Wall -Wextra)
endif()

# Set output directory (optional)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Print OpenCV info for debugging
message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")