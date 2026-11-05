cmake_minimum_required(VERSION 4.0)
project(TOC_P1)

set(CMAKE_CXX_STANDARD 20)

# 1. Fetch the GLFW windowing framework dynamically
include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

# 2. Define directory paths for the GUI libraries
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMNODES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imnodes)

# 3. Capture all GUI translation units using GLOB patterns
file(GLOB GUI_SOURCES
        ${IMGUI_DIR}/*.cpp
        ${IMNODES_DIR}/*.cpp
)

# 4. Compile the target executable (TOC_P1.cpp is now the sole entry point)
add_executable(TOC_P1 TOC_P1.cpp ${GUI_SOURCES})

# 5. Provide the compiler with the include directories for header resolution
target_include_directories(TOC_P1 PRIVATE
        ${IMGUI_DIR}
        ${IMNODES_DIR}
)

# 6. Link the hardware acceleration and window manager APIs
target_link_libraries(TOC_P1 PRIVATE glfw opengl32)