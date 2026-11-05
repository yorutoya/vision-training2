#pragma once
#include <opencv2/core.hpp>
#include <string>
struct CameraCalibration
{
    cv::Mat camera_matrix;
    cv::Mat dist_coeffs;
    cv::Size image_size;
};

CameraCalibration load_calibration(const std::string& path);
