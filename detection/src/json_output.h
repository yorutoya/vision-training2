#pragma once
#include "camera_transform.h"
#include "postprocess.h"
#include <ostream>
#include <vector>
struct DetectionResult
{
    Detection detection;
    std::optional<CameraPose> pose;
};

void write_frame_json(std::ostream& output, int frame_index,
                      const std::vector<DetectionResult>& detections);
