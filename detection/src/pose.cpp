#include "pose.h"

#include <opencv2/calib3d.hpp>
#include "armor_geometry.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

std::optional<PoseResult> estimate_pose(
    const Detection& detection,
    const CameraCalibration& calibration)
{

    if (detection.pattern_id < 0 || detection.pattern_id > 8)
        return std::nullopt;

    const auto object_points = armor_object_points(detection.pattern_id);

    std::vector<cv::Point2d> image_points;
    for (const auto& point : detection.corners)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            return std::nullopt;
        image_points.emplace_back(point.x, point.y);
    }

    try
    {
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;

        cv::solvePnPGeneric(
            object_points, image_points,
            calibration.camera_matrix, calibration.dist_coeffs,
            rvecs, tvecs, false, cv::SOLVEPNP_IPPE
        );

        std::optional<PoseResult> best;
        double best_error = std::numeric_limits<double>::infinity();

        for (std::size_t i = 0; i < rvecs.size(); ++i)
        {

            if (!cv::checkRange(rvecs[i]) || !cv::checkRange(tvecs[i]))
                continue;

            cv::Mat rotation;

            cv::Rodrigues(rvecs[i], rotation);

            bool in_front = true;
            for (const auto& point : object_points)
            {

                const double z =
                    rotation.at<double>(2, 0) * point.x +
                    rotation.at<double>(2, 1) * point.y +
                    rotation.at<double>(2, 2) * point.z +
                    tvecs[i].at<double>(2, 0);

                if (!std::isfinite(z) || z <= 0.0)
                {
                    in_front = false;
                    break;
                }
            }
            if (!in_front)
                continue;

            std::vector<cv::Point2d> projected;

            cv::projectPoints(
                object_points, rvecs[i], tvecs[i],
                calibration.camera_matrix, calibration.dist_coeffs,
                projected
            );

            double squared_error = 0.0;
            for (std::size_t k = 0; k < image_points.size(); ++k)
            {
                const auto difference = projected[k] - image_points[k];
                squared_error += difference.dot(difference);
            }
            const double rms = std::sqrt(squared_error / 4.0);

            if (std::isfinite(rms) && rms < best_error)
            {
                best_error = rms;

                best = PoseResult{rvecs[i].clone(), tvecs[i].clone(), rms};
            }
        }
        return best;
    }

    catch (const cv::Exception&)
    {

        return std::nullopt;
    }
}
