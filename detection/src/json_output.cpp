#include "json_output.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <stdexcept>
void write_frame_json(std::ostream& output, int frame_index,
                      const std::vector<DetectionResult>& detections)
{

    static const std::array<const char*, 4> colors{"blue", "red", "neutral", "purple"};
    static const std::array<const char*, 9> patterns{
        "sentry", "1", "2", "3", "4", "5", "outpost", "base_small", "base_big"};
    if (frame_index < 0) throw std::runtime_error("Negative frame index");
    nlohmann::json frame{{"frame_index", frame_index}, {"timestamp_s", frame_index / 30.0},
                         {"detections", nlohmann::json::array()}};
    for (const auto& result : detections)
    {
        const auto& d = result.detection;
        if (d.color_id < 0 || d.color_id >= 4 || d.pattern_id < 0 || d.pattern_id >= 9 ||
            !std::isfinite(d.confidence) || d.confidence < 0 || d.confidence > 1)
            throw std::runtime_error("Invalid detection metadata");

        nlohmann::json item{{"color", colors[d.color_id]}, {"pattern", patterns[d.pattern_id]},
            {"confidence", d.confidence}, {"corners_px", nlohmann::json::array()},
            {"corners_camera_m", nullptr}, {"reprojection_error_px", nullptr}};
        for (const auto& p : d.corners)
        {
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                throw std::runtime_error("Non-finite image corner");
            item["corners_px"].push_back({p.x, p.y});
        }
        if (result.pose)
        {
            const auto& pose = *result.pose;
            bool valid = std::isfinite(pose.reprojection_error_px) && pose.reprojection_error_px >= 0;
            for (const auto& p : pose.corners)
                valid = valid && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && p.z > 0;
            if (valid)
            {
                item["corners_camera_m"] = nlohmann::json::array();
                for (const auto& p : pose.corners) item["corners_camera_m"].push_back({p.x, p.y, p.z});
                item["reprojection_error_px"] = pose.reprojection_error_px;
            }
        }
        frame["detections"].push_back(item);
    }

    output << frame.dump() << '\n';
    if (!output) throw std::runtime_error("Failed to write JSONL output");
}
