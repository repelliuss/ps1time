cmake_minimum_required(VERSION 3.1)

project(ps1emu VERSION 0.0 LANGUAGES CXX)

# # import custom modules path
# list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SDL2)

aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
add_executable(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set(OpenGL_GL_PREFERENCE LEGACY)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 OpenGL::GL)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${INCLUDE_DIRS}
  PRIVATE SDL2::SDL2 OpenGL::GL)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_compile_options(${PROJECT_NAME} PRIVATE -g -DDEBUG)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ..
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)
