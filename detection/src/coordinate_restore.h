#pragma once
#include "postprocess.h"
#include "preprocess.h"

void restore_coordinates(std::vector<Detection>& detections, const PreprocessedImage& input);
