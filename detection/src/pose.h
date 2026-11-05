#pragma once

#include "postprocess.h"
#include "calibration.h"

#include <opencv2/core.hpp>
#include <optional>
#include <string>

struct PoseResult
{

    cv::Mat rvec;

    cv::Mat tvec;

    double reprojection_error_px;
};

std::optional<PoseResult> estimate_pose(
    const Detection& detection,
    const CameraCalibration& calibration
);
