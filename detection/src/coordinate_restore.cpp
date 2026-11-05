#include "coordinate_restore.h"
#include <cmath>
#include <stdexcept>
void restore_coordinates(std::vector<Detection>& detections, const PreprocessedImage& input)
{

    if (!std::isfinite(input.scale) || input.scale <= 0)
        throw std::runtime_error("Invalid preprocessing scale");

    for (auto& detection : detections)
        for (auto& point : detection.corners)
        {
            point.x = static_cast<float>((point.x - input.pad_left) / input.scale);
            point.y = static_cast<float>((point.y - input.pad_top) / input.scale);
        }
}
