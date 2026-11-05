#pragma once

#include "preprocess.h"
#include <vector>

std::vector<float> run_inference(
  Ort::Session& session,
  PreprocessedImage& image
);
