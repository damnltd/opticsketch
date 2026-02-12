cmake_minimum_required(VERSION 3.15)
project(OpticSketch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Find packages
find_package(OpenGL REQUIRED)
if(NOT OpenGL_FOUND)
    if(WIN32)
        message(FATAL_ERROR "OpenGL not found. OpenGL should be available through your graphics drivers on Windows.")
    else()
        message(FATAL_ERROR "OpenGL not found. Please install libgl1-mesa-dev:\n  sudo apt-get install libgl1-mesa-dev")
    endif()
endif()

find_package(glfw3 QUIET)

# If GLFW3 is not found via find_package, use FetchContent
if(NOT glfw3_FOUND)
    message(STATUS "GLFW3 not found via find_package, using FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    # Create alias for consistency
    if(NOT TARGET glfw3)
        add_library(glfw3 ALIAS glfw)
    endif()
else()
    message(STATUS "GLFW3 found via find_package")
endif()

find_package(glm QUIET)

# If GLM is not found via find_package, use FetchContent
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
    )
    # GLM 0.9.9.8 uses old cmake_minimum_required; CMake 4+ requires policy >= 3.5
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for subprojects (GLM compatibility)")
    FetchContent_MakeAvailable(glm)
endif()

# GLAD - OpenGL loader
# GLAD - OpenGL loader
# We generate the loader via Python (pip package), no need to add glad as a CMake subproject
# GLAD requires Python to generate loader files
# We'll use FetchContent to get GLAD and generate the loader
#include(FetchContent)
#FetchContent_Declare(
#    glad
#    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
#    GIT_TAG v0.1.36
#)
#FetchContent_MakeAvailable(glad)

# Generate GLAD loader for OpenGL 3.3 core profile
# Requires: pip install glad
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GLAD_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/glad")
set(GLAD_INCLUDE_DIR "${GLAD_OUTPUT_DIR}/include")
set(GLAD_SRC_DIR "${GLAD_OUTPUT_DIR}/src")

# Generate GLAD files using Python module
# Note: GLAD generates glad.c (not gl.c)
set(GLAD_HEADER ${GLAD_INCLUDE_DIR}/glad/glad.h)
set(GLAD_SOURCE ${GLAD_SRC_DIR}/glad.c)

# Generate GLAD files - this must run before compilation
add_custom_command(
    OUTPUT ${GLAD_HEADER} ${GLAD_SOURCE}
    COMMAND ${Python3_EXECUTABLE} -m glad
        --generator=c
        --spec=gl
        --api=gl=3.3
        --profile=core
        --out-path=${GLAD_OUTPUT_DIR}
        --reproducible
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating GLAD loader for OpenGL 3.3 core"
)

# Create GLAD library (C library)
# CMake will automatically wait for the OUTPUT files from add_custom_command
add_library(glad_gl_core_33 STATIC
    ${GLAD_SOURCE}
)

# Set C standard for GLAD
set_target_properties(glad_gl_core_33 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_include_directories(glad_gl_core_33 PUBLIC
    ${GLAD_INCLUDE_DIR}
)

# ImGui - using docking branch for docking and viewport support
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# ImGui sources
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Enable docking and viewports
target_compile_definitions(imgui_lib PUBLIC
    IMGUI_ENABLE_DOCKING=1
)
target_link_libraries(imgui_lib PUBLIC
    glfw
    OpenGL::GL
)

# tinyfiledialogs - native open/save file dialog
FetchContent_Declare(
    tinyfiledialogs
    GIT_REPOSITORY https://github.com/opensource-mirrors/tinyfiledialogs.git
    GIT_TAG master
)
FetchContent_MakeAvailable(tinyfiledialogs)
target_include_directories(tinyfiledialogs PUBLIC ${tinyfiledialogs_SOURCE_DIR})

# stb_image_write - PNG export
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# tinyobjloader - OBJ mesh import
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)

# Executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ui/theme.cpp
    src/ui/library_panel.cpp
    src/ui/toolbox_panel.cpp
    src/ui/outliner_panel.cpp
    src/ui/properties_panel.cpp
    src/camera/camera.cpp
    src/render/raycast.cpp
    src/render/gizmo.cpp
    src/render/shader.cpp
    src/render/viewport.cpp
    src/render/beam.cpp
    src/render/mesh_loader.cpp
    src/undo/undo.cpp
    src/export/export_png.cpp
    src/elements/element.cpp
    src/elements/basic_elements.cpp
    src/elements/annotation.cpp
    src/style/scene_style.cpp
    src/input/shortcut_manager.cpp
    src/ui/style_editor_panel.cpp
    src/ui/shortcuts_panel.cpp
    src/ui/template_panel.cpp
    src/templates/templates.cpp
    src/scene/scene.cpp
    src/project/project.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${GLAD_INCLUDE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

# Enable ImGui docking
target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_ENABLE_DOCKING=1
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    glad_gl_core_33
    imgui_lib
    glm::glm
    tinyfiledialogs
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WINDOWS)
endif()

# Copy assets directory to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

