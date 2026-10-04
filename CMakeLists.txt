cmake_minimum_required(VERSION 3.21)
project(Graphixxx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Triplet statique sous Windows => runtime MSVC statique
if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES "static$")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")

add_executable(Graphixxx
  src/app.cpp
  src/gif_exporter.cpp
  src/image_loader.cpp
  src/main.cpp
  src/renderer.cpp
  src/shader_editor.cpp
  src/TextEditor.cpp
  src/uniforms.cpp
)

target_include_directories(Graphixxx PRIVATE
  src
  ${STB_INCLUDE_DIRS}
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(Graphixxx PRIVATE ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(Graphixxx PRIVATE
  glfw
  glad::glad
  imgui::imgui
  ${FFMPEG_LIBRARIES}
)

if(WIN32)
  target_compile_definitions(Graphixxx PRIVATE NOMINMAX)
  target_link_libraries(Graphixxx PRIVATE comdlg32)
endif()

install(TARGETS Graphixxx RUNTIME DESTINATION .)
install(DIRECTORY shaders DESTINATION .)
