#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

bool read_next_frame(cv::VideoCapture& video, cv::Mat& frame);
