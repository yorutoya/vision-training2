#include "video_read.h"
bool read_next_frame(cv::VideoCapture& video, cv::Mat& frame)
{

    return video.read(frame);
}
