#pragma once

#include <opencv2/core.hpp>
#include <array>
#include <vector>

struct Detection
{

    std::array<cv::Point2f, 4> corners;

    float confidence;

    int color_id;

    int pattern_id;
};

std::vector<Detection> postprocess(
    const std::vector<float>& predictions,
    float confidence_threshold = 0.5f,
    float nms_threshold = 0.5f
);
