#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <vector>

struct PreprocessedImage
{

    std::vector<Ort::Float16_t> data;

    double scale;

    int pad_left;
    int pad_top;
};

PreprocessedImage preprocess(const cv::Mat& frame);
