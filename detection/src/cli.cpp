#include "cli.h"
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace
{

float threshold(const std::string& text)
{
    std::size_t consumed = 0;
    const float value = std::stof(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value < 0 || value > 1)
        throw std::runtime_error("Threshold must be a finite number within [0, 1]");
    return value;
}
}
ProgramOptions parse_arguments(int argc, char** argv)
{
    ProgramOptions result;
    std::set<std::string> seen;

    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") { result.help = true; continue; }
        if (key.empty()) throw std::runtime_error("Empty argument");
        if (key[0] != '-')
        {
            if (!result.video.empty()) throw std::runtime_error("Only one video is allowed");
            result.video = key;
            continue;
        }
        if (key != "--model" && key != "--video" && key != "--calibration" &&
            key != "--output" && key != "--confidence" && key != "--nms")
            throw std::runtime_error("Unknown option: " + key);
        if (!seen.insert(key).second) throw std::runtime_error("Duplicate option: " + key);
        if (++i >= argc || std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0)
            throw std::runtime_error("Missing value for " + key);
        const std::string value = argv[i];
        if (key == "--model") result.model = value;
        else if (key == "--calibration") result.calibration = value;
        else if (key == "--output") result.output = value;
        else if (key == "--confidence") result.confidence = threshold(value);
        else if (key == "--nms") result.nms = threshold(value);
        else
        {
            if (!result.video.empty()) throw std::runtime_error("Only one video is allowed");
            result.video = value;
        }
    }

    if (result.help) return result;
    if (result.video.empty()) throw std::runtime_error("A video path is required");

    if (result.output.empty())
        result.output = std::filesystem::path(result.video).stem().string() + ".jsonl";
    return result;
}
std::string usage(const std::string& program)
{
    return "Usage: " + program +
        " --video VIDEO [--model MODEL] [--calibration YAML] [--output JSONL]\n"
        "       [--confidence 0.5] [--nms 0.5]\n"
        "Legacy: " + program + " VIDEO\n"
        "Defaults: assets/0526.onnx, assets/calibration.yaml, <video-stem>.jsonl\n"
        "Input: video matching calibration resolution, timestamps use 30 FPS.\n"
        "An existing output file is replaced. Use a different path to keep it.\n";
}
void validate_output_path(const ProgramOptions& options)
{
    namespace fs = std::filesystem;

    for (const auto& input : {options.video, options.model, options.calibration})
    {
        if (fs::weakly_canonical(options.output) == fs::weakly_canonical(input) ||
            (fs::exists(options.output) && fs::exists(input) && fs::equivalent(options.output, input)))
            throw std::runtime_error("Output path must differ from every input path");
    }
}
