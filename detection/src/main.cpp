#include "cli.h"
#include "calibration.h"
#include "video_read.h"
#include "preprocess.h"
#include "inference.h"
#include "postprocess.h"
#include "coordinate_restore.h"
#include "pose.h"
#include "camera_transform.h"
#include "json_output.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    try
    {

        const auto config = parse_arguments(argc, argv);
        if (config.help) { std::cout << usage(argv[0]); return 0; }
        validate_output_path(config);

        const auto calibration = load_calibration(config.calibration);
        cv::VideoCapture video(config.video);
        if (!video.isOpened()) throw std::runtime_error("Cannot open video: " + config.video);

        const double expected_frames = video.get(cv::CAP_PROP_FRAME_COUNT);

        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "vision_detection");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2);
        Ort::Session session(env, config.model.c_str(), options);

        cv::Mat frame;
        if (!read_next_frame(video, frame)) throw std::runtime_error("Video has no readable frames");
        if (frame.size() != calibration.image_size)
            throw std::runtime_error("Video dimensions do not match calibration");
        std::ofstream output(config.output);
        if (!output) throw std::runtime_error("Cannot open output: " + config.output);
        int frame_index = 0;
        std::size_t detection_count = 0, pose_failures = 0;

        do
        {
            if (frame.size() != calibration.image_size)
                throw std::runtime_error("Frame dimensions changed during video");

            auto input = preprocess(frame);

            auto predictions = run_inference(session, input);

            auto detections = postprocess(predictions, config.confidence, config.nms);

            restore_coordinates(detections, input);

            std::vector<DetectionResult> results;
            results.reserve(detections.size());
            for (const auto& detection : detections)
            {

                const auto pose = estimate_pose(detection, calibration);

                const auto camera_pose = transform_corners(detection.pattern_id, pose);

                if (!camera_pose) ++pose_failures;
                results.push_back({detection, camera_pose});
            }

            write_frame_json(output, frame_index, results);
            detection_count += results.size();
            ++frame_index;
        } while (read_next_frame(video, frame));

        output.close();
        if (!output) throw std::runtime_error("Failed to finish JSONL output");
        if (std::isfinite(expected_frames) && expected_frames > 0 &&
            std::abs(expected_frames - frame_index) > 0.5)
            throw std::runtime_error("Decoded frame count differs from video metadata; output may be partial");
        std::cout << "Read " << frame_index << " frames, " << detection_count
                  << " detections, " << pose_failures << " pose failures. Output: "
                  << config.output << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {

        std::cerr << "Detection failed: " << error.what() << '\n';
        return 1;
    }
}
