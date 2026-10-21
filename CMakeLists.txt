cmake_minimum_required(VERSION 3.20)

set(CMAKE_TLS_VERIFY OFF CACHE BOOL "" FORCE)

project(WitchEngine C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW Setup
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
    TLS_VERIFY OFF
)
FetchContent_MakeAvailable(glfw)

# GLM Header-Only Setup
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
    TLS_VERIFY OFF
)
FetchContent_MakeAvailable(glm)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/glad.c
)

# GLFW_INCLUDE_NONE sadece bizim hedefimize özel eklenir (GLFW derlemesini bozmaz)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)
