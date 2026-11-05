#include "postprocess.h"

#include <opencv2/dnn.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{

    float sigmoid(float value)
    {

        if (value >= 0.0f)
            return 1.0f / (1.0f + std::exp(-value));

        const float e = std::exp(value);
        return e / (1.0f + e);
    }
}

std::vector<Detection> postprocess(
    const std::vector<float>& predictions,
    float confidence_threshold,
    float nms_threshold)
{

    constexpr int row_count = 25200;
    constexpr int row_size = 22;

    if (predictions.size() != row_count * row_size)
        throw std::runtime_error("Unexpected prediction size");

    if (!std::isfinite(confidence_threshold) ||
        !std::isfinite(nms_threshold) ||
        confidence_threshold < 0.0f || confidence_threshold > 1.0f ||
        nms_threshold < 0.0f || nms_threshold > 1.0f)
    {
        throw std::runtime_error("Threshold must be within [0, 1]");
    }

    std::vector<Detection> candidates;
    std::vector<cv::Rect2d> boxes;
    std::vector<float> scores;

    for (int i = 0; i < row_count; i++)
    {

        const float* row = predictions.data() + i * row_size;

        if (!std::all_of(row, row + row_size, [](float v) {return std::isfinite(v);}))
        {
            continue;
        }

        const float confidence = sigmoid(row[8]);

        if (confidence <= confidence_threshold)
        {
            continue;
        }

        Detection detection{};
        detection.confidence = confidence;

        detection.color_id = static_cast<int>(
            std::max_element(row + 9, row + 13) - (row + 9)
            );

        detection.pattern_id = static_cast<int>(
            std::max_element(row + 13, row + 22) - (row + 13)
        );

        for (int k = 0; k < 4; k++)
        {
            detection.corners[k] = cv::Point2f(
                row[2 * k],
                row[2 * k + 1]
            );
        }

        double left = detection.corners[0].x;
        double right = left;
        double top = detection.corners[0].y;
        double bottom = top;

        for (const auto& point: detection.corners)
        {
            left = std::min(left, static_cast<double>(point.x));
            right = std::max(right, static_cast<double>(point.x));
            top = std::min(top, static_cast<double>(point.y));
            bottom = std::max(bottom, static_cast<double>(point.y));
        }

        if (right <= left || bottom <= top)
            continue;

        candidates.push_back(detection);
        boxes.emplace_back(left, top, right - left, bottom - top);
        scores.push_back(confidence);
    }

    std::vector<int> kept;

    if (!candidates.empty())
    {
        cv::dnn::NMSBoxes(
            boxes, scores,
            confidence_threshold, nms_threshold,
            kept
        );
    }

    std::vector<Detection> result;

    result.reserve(kept.size());

    for (int index : kept)
        result.push_back(candidates[index]);

    return result;
}
