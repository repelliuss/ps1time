cmake_minimum_required(VERSION 3.1)

project(ps1emu VERSION 0.0 LANGUAGES CXX)

aux_source_directory(src SRC_LIST)
set(INCLUDE_DIRS include)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${INCLUDE_DIRS})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_compile_options(${PROJECT_NAME} PRIVATE -g)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ..
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)
