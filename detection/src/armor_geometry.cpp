#include "armor_geometry.h"
#include <stdexcept>
std::vector<cv::Point3d> armor_object_points(int pattern_id)
{
    if (pattern_id < 0 || pattern_id > 8)
        throw std::runtime_error("Unknown armor pattern");

    const double width = (pattern_id == 1 || pattern_id == 8) ? 0.230 : 0.135;
    const double height = 0.056;

    return {{-width/2, -height/2, 0}, {-width/2, height/2, 0},
            {width/2, height/2, 0}, {width/2, -height/2, 0}};
}
