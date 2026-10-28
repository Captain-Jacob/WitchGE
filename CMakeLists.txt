cmake_minimum_required(VERSION 3.16)
project(WitchEngine CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Yerel Header Dizinleri (Config, Player, glad.h, KHR)
include_directories(include)

# ONNX Runtime Başlık Dosyaları
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/include")
    include_directories(third_party/onnxruntime/include)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib)
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# 1. GLFW Otomatik İndirme
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. GLM Otomatik İndirme
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Kaynak Dosyaları Topla
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

# Çalıştırılabilir Hedefi Oluştur
add_executable(WitchEngine ${SOURCES})

# GLFW'nin gl.h'ı yüklemesini SADECE bizim projemiz için engelle (GLFW'nin kendi derlemesini etkilemez)
target_compile_definitions(WitchEngine PRIVATE GLFW_INCLUDE_NONE)

# Kütüphaneleri Bağla
target_link_libraries(WitchEngine PRIVATE OpenGL::GL glfw glm::glm)

# ONNX Runtime mevcutsa bağla
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib")
    target_link_libraries(WitchEngine PRIVATE onnxruntime)
endif()