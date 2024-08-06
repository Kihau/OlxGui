cmake_minimum_required(VERSION 3.5)
project(OlxGui)

# set(CMAKE_C_COMPILER   "clang")
# set(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR "GNU")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -O0 -ggdb")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "/Wall /Zc:preprocessor")
# elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
endif()

add_subdirectory(lib/glfw)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

add_library(
    ImGUI 
    STATIC 
    
    lib/imgui/imgui.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/misc/cpp/imgui_stdlib.cpp
    lib/imgui/backends/imgui_impl_opengl3.cpp
    lib/imgui/backends/imgui_impl_glfw.cpp
)

target_include_directories(ImGUI PUBLIC lib/imgui)
target_link_libraries(ImGUI glfw)


# set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
# set(BUILD_SHARED_LIBS OFF)
# set(CMAKE_EXE_LINKER_FLAGS "-static")

configure_file(res/Roboto-Regular.ttf Roboto-Regular.ttf COPYONLY)

# OlxFind source code not available on github yet!
# configure_file(res/olx-find olx-find COPYONLY)
# configure_file(res/olx-find.exe olx-find.exe COPYONLY)

add_executable(
    OlxGui 
    src/main.cpp
)

target_link_libraries(OlxGui OpenGL::GL)
target_link_libraries(OlxGui glfw)
target_link_libraries(OlxGui ImGUI)

target_include_directories(OlxGui PRIVATE lib)
target_include_directories(OlxGui PRIVATE src)
