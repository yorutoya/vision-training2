#include "calibration.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

CameraCalibration load_calibration(const std::string& path)
{

    const auto yaml = YAML::LoadFile(path);
    const auto matrix = yaml["camera_matrix"];
    const auto distortion = yaml["distort_coeffs"];

    if (!matrix.IsSequence() || matrix.size() != 3 ||
        !distortion.IsSequence() || distortion.size() != 5)
    {
        throw std::runtime_error("Invalid calibration dimensions");
    }

    CameraCalibration result;

    result.image_size = cv::Size(yaml["image_width"].as<int>(), yaml["image_height"].as<int>());
    if (result.image_size.width <= 0 || result.image_size.height <= 0)
        throw std::runtime_error("Invalid calibration image dimensions");

    result.camera_matrix = cv::Mat(3, 3, CV_64F);
    result.dist_coeffs = cv::Mat(1, 5, CV_64F);

    for (int row = 0; row < 3; ++row)
    {
        if (!matrix[row].IsSequence() || matrix[row].size() != 3)
            throw std::runtime_error("Invalid camera matrix");

        for (int col = 0; col < 3; ++col)
        {

            result.camera_matrix.at<double>(row, col) =
                matrix[row][col].as<double>();
        }
    }

    for (int i = 0; i < 5; ++i)
        result.dist_coeffs.at<double>(0, i) = distortion[i].as<double>();

    if (!cv::checkRange(result.camera_matrix) ||
        !cv::checkRange(result.dist_coeffs) ||
        result.camera_matrix.at<double>(0, 0) <= 0.0 ||
        result.camera_matrix.at<double>(1, 1) <= 0.0)
    {
        throw std::runtime_error("Invalid calibration values");
    }

    return result;
}
