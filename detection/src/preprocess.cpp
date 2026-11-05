#include "preprocess.h"

#include<opencv2/imgproc.hpp>
#include<algorithm>
#include<cmath>
#include<stdexcept>

PreprocessedImage preprocess(const cv::Mat& frame)
{

    if (frame.empty() || frame.type() != CV_8UC3)
        throw std::runtime_error("Expected a non-empty BGR image");

    constexpr int input_size = 640;
    PreprocessedImage result;

    result.scale = std::min(
        static_cast<double>(input_size) / frame.cols,
        static_cast<double>(input_size) / frame.rows
        );

    int resized_width = std::clamp(
        static_cast<int>(std::round(frame.cols * result.scale)),
        1, input_size
    );
    int resized_height = std::clamp(
        static_cast<int>(std::round(frame.rows * result.scale)),
        1, input_size
    );

    cv::Mat resized;

    cv::resize(frame, resized, cv::Size(resized_width, resized_height));

    result.pad_left = (input_size - resized_width) / 2;
    result.pad_top = (input_size - resized_height) / 2;

    cv::Mat padded;

    cv::copyMakeBorder(
        resized, padded,
        result.pad_top,
        input_size - resized_height - result.pad_top,
        result.pad_left,
        input_size - resized_width - result.pad_left,
        cv::BORDER_CONSTANT,
        cv::Scalar(114, 114, 114)
        );

    cv::Mat rgb;

    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

    constexpr int plane_size = input_size * input_size;
    result.data.resize(3 * plane_size);

    for (int y = 0; y < input_size; y++)
    {

        const cv::Vec3b* row = rgb.ptr<cv::Vec3b>(y);

        for (int x = 0; x < input_size; x++)
        {
            for (int c = 0; c < 3; c++)
            {

                int index = c * plane_size + y * input_size + x;

                result.data[index] = Ort::Float16_t(row[x][c] / 255.0f);
            }
        }
    }

    return result;
}
