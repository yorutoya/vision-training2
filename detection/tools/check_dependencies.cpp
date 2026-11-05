#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <onnxruntime_cxx_api.h>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{

    try {

        const std::string assets = argc > 1 ? argv[1] : VISION_ASSET_DIR;

        nlohmann::json report;
        report["opencv"] = CV_VERSION;
        report["onnxruntime"] = Ort::GetVersionString();

        const auto calibration = YAML::LoadFile(assets + "/calibration.yaml");
        const int width = calibration["image_width"].as<int>();
        const int height = calibration["image_height"].as<int>();

        cv::Mat camera(3, 3, CV_64F);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                camera.at<double>(row, col) = calibration["camera_matrix"][row][col].as<double>();
        const auto distortion = calibration["distort_coeffs"].as<std::vector<double>>();
        if (distortion.size() != 5 || !cv::checkRange(camera))
            throw std::runtime_error("Invalid calibration data");
        report["calibration"] = "OK";

        for (const std::string name : {"small.mp4", "large.mp4"}) {
            cv::VideoCapture video(assets + "/" + name);
            if (!video.isOpened())
                throw std::runtime_error("Cannot open " + name);
            cv::Mat frame;
            int count = 0;
            while (video.read(frame)) {
                if (frame.cols != width || frame.rows != height)
                    throw std::runtime_error("Unexpected frame dimensions: " + name);
                ++count;
            }

            if (count != 360)
                throw std::runtime_error("Incomplete fixture video: " + name);
            report["video_frames"][name] = count;
        }

        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "dependency_check");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2);
        Ort::Session session(env, (assets + "/0526.onnx").c_str(), options);

        const auto input_type = session.GetInputTypeInfo(0);
        const auto input_info = input_type.GetTensorTypeAndShapeInfo();
        const std::array<int64_t, 4> shape{1, 3, 640, 640};
        if (input_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 ||
            input_info.GetShape() != std::vector<int64_t>(shape.begin(), shape.end()))
            throw std::runtime_error("Unexpected model input type or shape");

        std::vector<Ort::Float16_t> pixels(1 * 3 * 640 * 640, Ort::Float16_t(0.0f));
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        auto input = Ort::Value::CreateTensor<Ort::Float16_t>(
            memory, pixels.data(), pixels.size(), shape.data(), shape.size());
        const char* input_names[] = {"images"};
        const char* output_names[] = {"output"};
        auto outputs = session.Run(Ort::RunOptions{nullptr}, input_names, &input, 1,
                                   output_names, 1);

        const auto output_info = outputs.at(0).GetTensorTypeAndShapeInfo();
        if (output_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            output_info.GetShape() != std::vector<int64_t>{1, 25200, 22})
            throw std::runtime_error("Unexpected model output type or shape");
        report["model_cpu_inference"] = "OK";
        report["output_shape"] = output_info.GetShape();

        std::cout << report.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dependency check failed: " << error.what() << '\n';
        return 1;
    }
}
