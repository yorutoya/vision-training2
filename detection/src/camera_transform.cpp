#include "camera_transform.h"
#include "armor_geometry.h"
#include <opencv2/calib3d.hpp>
#include <cmath>
std::optional<CameraPose> transform_corners(int pattern_id, const std::optional<PoseResult>& pose)
{

    if (!pose || pattern_id < 0 || pattern_id > 8 ||
        !std::isfinite(pose->reprojection_error_px) || pose->reprojection_error_px < 0)
        return std::nullopt;
    if (pose->rvec.type() != CV_64F || pose->tvec.type() != CV_64F ||
        pose->rvec.rows != 3 || pose->rvec.cols != 1 ||
        pose->tvec.rows != 3 || pose->tvec.cols != 1 ||
        !cv::checkRange(pose->rvec) || !cv::checkRange(pose->tvec))
        return std::nullopt;
    try
    {

        cv::Mat rotation;
        cv::Rodrigues(pose->rvec, rotation);
        const auto object_points = armor_object_points(pattern_id);
        CameraPose result{};
        result.reprojection_error_px = pose->reprojection_error_px;

        for (std::size_t i = 0; i < object_points.size(); ++i)
        {
            const auto& p = object_points[i];
            cv::Vec3d q;
            for (int row = 0; row < 3; ++row)
                q[row] = rotation.at<double>(row, 0)*p.x +
                         rotation.at<double>(row, 1)*p.y +
                         rotation.at<double>(row, 2)*p.z + pose->tvec.at<double>(row, 0);
            if (!std::isfinite(q[0]) || !std::isfinite(q[1]) || !std::isfinite(q[2]) || q[2] <= 0)
                return std::nullopt;
            result.corners[i] = cv::Point3d(q[0], q[1], q[2]);
        }
        return result;
    }
    catch (const cv::Exception&) { return std::nullopt; }
}
