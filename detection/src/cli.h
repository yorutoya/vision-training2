#pragma once
#include <string>
struct ProgramOptions
{
    std::string model = "assets/0526.onnx";
    std::string video;
    std::string calibration = "assets/calibration.yaml";
    std::string output;
    float confidence = 0.5f;
    float nms = 0.5f;
    bool help = false;
};

ProgramOptions parse_arguments(int argc, char** argv);
std::string usage(const std::string& program);

void validate_output_path(const ProgramOptions& options);
