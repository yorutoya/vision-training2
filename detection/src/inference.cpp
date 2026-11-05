#include "inference.h"

#include <array>
#include <stdexcept>

std::vector<float> run_inference(
    Ort::Session& session,
    PreprocessedImage& image)
{

    const std::array<int64_t, 4> shape{1, 3, 640, 640};

    if (image.data.size() != 3 * 640 * 640)
        throw std::runtime_error("Unexpected input data size");

    auto memory = Ort::MemoryInfo::CreateCpu(
      OrtArenaAllocator, OrtMemTypeDefault
    );

    auto input_tensor = Ort::Value::CreateTensor<Ort::Float16_t>(
        memory,
        image.data.data(),
        image.data.size(),
        shape.data(),
        shape.size()
        );

    const char* input_names[] = {"images"};
    const char* output_names[] = {"output"};

    auto outputs = session.Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_names,
        1
    );

    const auto info = outputs.at(0).GetTensorTypeAndShapeInfo();

    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        info.GetShape() != std::vector<int64_t>{1, 25200, 22})
    {
        throw std::runtime_error("Unexpected model output");
    }

    const float* values = outputs[0].GetTensorData<float>();

    return std::vector<float>(
        values,
        values + info.GetElementCount()
    );
}
