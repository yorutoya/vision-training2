include_guard(GLOBAL)

if(APPLE)
    find_program(HOMEBREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
    if(HOMEBREW_EXECUTABLE)
        execute_process(COMMAND "${HOMEBREW_EXECUTABLE}" --prefix
            OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
        list(PREPEND CMAKE_PREFIX_PATH
            "${HOMEBREW_PREFIX}/opt/opencv@4" "${HOMEBREW_PREFIX}")
    endif()
endif()

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio highgui calib3d dnn)
find_package(onnxruntime CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(vision_dependencies INTERFACE)
target_compile_features(vision_dependencies INTERFACE cxx_std_17)
target_include_directories(vision_dependencies INTERFACE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(vision_dependencies INTERFACE
    ${OpenCV_LIBS}
    onnxruntime::onnxruntime
    yaml-cpp::yaml-cpp
    nlohmann_json::nlohmann_json
)
