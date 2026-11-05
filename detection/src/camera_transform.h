#pragma once
#include "pose.h"
#include <array>
#include <optional>
struct CameraPose
{
    std::array<cv::Point3d, 4> corners;
    double reprojection_error_px;
};

std::optional<CameraPose> transform_corners(int pattern_id, const std::optional<PoseResult>& pose);
