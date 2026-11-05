cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(vision_detection LANGUAGES CXX)
endif()

include(cmake/Dependencies.cmake)

set(VISION_MODULE_SOURCES
    src/video_read.cpp
    src/video_read.h
    src/preprocess.cpp
    src/preprocess.h
    src/inference.cpp
    src/inference.h
    src/postprocess.cpp
    src/postprocess.h
    src/pose.cpp
    src/pose.h
    src/cli.cpp
    src/cli.h
    src/calibration.cpp
    src/calibration.h
    src/coordinate_restore.cpp
    src/coordinate_restore.h
    src/armor_geometry.cpp
    src/armor_geometry.h
    src/camera_transform.cpp
    src/camera_transform.h
    src/json_output.cpp
    src/json_output.h
)
add_executable(vision_detection src/main.cpp ${VISION_MODULE_SOURCES})
target_include_directories(vision_detection PRIVATE src)
target_link_libraries(vision_detection PRIVATE vision_dependencies)
set_target_properties(vision_detection PROPERTIES CXX_EXTENSIONS OFF)

add_executable(dependency_check EXCLUDE_FROM_ALL tools/check_dependencies.cpp)
target_link_libraries(dependency_check PRIVATE vision_dependencies)
target_compile_definitions(dependency_check PRIVATE
    VISION_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_executable(pipeline_check EXCLUDE_FROM_ALL tools/check_pipeline.cpp ${VISION_MODULE_SOURCES})
target_include_directories(pipeline_check PRIVATE src)
target_link_libraries(pipeline_check PRIVATE vision_dependencies)
target_compile_definitions(pipeline_check PRIVATE VISION_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
