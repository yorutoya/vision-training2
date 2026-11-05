# vision-detection3

[中文](README.md) · [English](README.en.md)

An offline armor-plate detector built with C++17, OpenCV, and ONNX Runtime. It processes
video frames, decodes detections, suppresses duplicates, estimates poses with PnP, and
exports 2D corners, camera-frame 3D corners, and reprojection errors as JSON Lines.

This project is based on the implementation in `vision-training2`, with source comments
removed and the modular structure retained. The completed implementation is the
`detection/` path. `aiming/` retains the original Rust simulator and student template;
it does not represent a completed predictive aiming strategy and is not built by CMake.

## Pipeline

| Step | Module | Responsibility |
| --- | --- | --- |
| 0 | `cli`, `calibration` | Parse paths and thresholds; load camera calibration |
| 1 | `video_read` | Decode video frames in order |
| 2.1 | `preprocess` | Letterbox, RGB, NCHW, normalization, and float16 conversion |
| 2.2 | `inference` | Run inference using a reusable ONNX session |
| 3 | `postprocess` | Decode corners, confidence, and classes; apply rectangular NMS |
| 4 | `coordinate_restore` | Undo scaling and padding to recover original-image coordinates |
| 5 | `armor_geometry`, `pose` | Select geometry from the pattern and estimate pose with IPPE |
| 6.1 | `camera_transform` | Apply `R × P + t` to each physical corner |
| 6.2 | `json_output` | Write one JSON Lines record per frame |

Each module has separate `.h/.cpp` files. `main.cpp` handles resource initialization,
execution order, and error reporting.

## Dependencies and build

Requirements: CMake 3.20+, a C++17 compiler, and:

- OpenCV 4: core, imgproc, imgcodecs, videoio, highgui, calib3d, and dnn.
- ONNX Runtime, yaml-cpp, and nlohmann-json.

On macOS with Homebrew:

```sh
brew install cmake opencv@4 onnxruntime yaml-cpp nlohmann-json
```

Build from the project root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target vision_detection --parallel 2
```

CMake discovers common Homebrew installation prefixes. For other layouts, set
`CMAKE_PREFIX_PATH` or the relevant package configuration directories. Inference uses
the CPU; neither CUDA nor a physical camera is required.

## Run

Run these commands from the project root:

```sh
mkdir -p detection/results

./build/detection/vision_detection \
  --model detection/assets/0526.onnx \
  --video detection/assets/small.mp4 \
  --calibration detection/assets/calibration.yaml \
  --output detection/results/small.jsonl

./build/detection/vision_detection \
  --model detection/assets/0526.onnx \
  --video detection/assets/large.mp4 \
  --calibration detection/assets/calibration.yaml \
  --output detection/results/large.jsonl
```

| Argument | Description | Default |
| --- | --- | --- |
| `--video` | Required input video; a single positional argument is also accepted | None |
| `--model` | ONNX model path | `assets/0526.onnx` |
| `--calibration` | Calibration YAML path | `assets/calibration.yaml` |
| `--output` | Output JSONL path | `<video-stem>.jsonl` in the working directory |
| `--confidence` | Confidence threshold in `[0,1]`; scores must be strictly greater | `0.5` |
| `--nms` | NMS IoU threshold in `[0,1]` | `0.5` |
| `--help` / `-h` | Display help without running inference | — |

All relative paths resolve against the **process working directory**. Default model and
calibration paths assume execution from `detection/`. Use the explicit paths above when
running from the project root.

Create the output parent directory first. An existing output is replaced when writing
starts, and a failure during processing may leave a partial file. Exit code `0` indicates
normal completion. The program rejects output paths that refer to the input model,
video, or calibration file.

## CLion

Open the root `CMakeLists.txt`, build, and select the `vision_detection` run target.
Set the working directory to the project root and use these program arguments:

```text
--model detection/assets/0526.onnx --video detection/assets/small.mp4 --calibration detection/assets/calibration.yaml --output small.jsonl
```

Alternatively, open `detection/CMakeLists.txt` directly, set the working directory to
`detection/`, and pass `assets/small.mp4`. The default output is then `small.jsonl` in
that working directory.

## Output contract

The output is UTF-8 JSON Lines: one complete JSON object per line, with no surrounding
array. Every frame is recorded, including frames with no detections. Frame indices start
at `0`, and `timestamp_s = frame_index / 30.0`.

```json
{"frame_index":0,"timestamp_s":0.0,"detections":[]}
```

Each detection contains:

| Field | Content |
| --- | --- |
| `color` | `blue`, `red`, `neutral`, or `purple` |
| `pattern` | `sentry`, `1`–`5`, `outpost`, `base_small`, or `base_big`; always a string |
| `confidence` | `sigmoid(objectness)`, without multiplication by class scores |
| `corners_px` | Four `[x,y]` points in original-image pixels |
| `corners_camera_m` | Four `[x,y,z]` points in camera coordinates, in meters; `null` on pose failure |
| `reprojection_error_px` | RMS of the four Euclidean reprojection errors; `null` on pose failure |

Corner order follows the plate's own orientation: **upper-left, lower-left, lower-right,
upper-right**. Camera axes are **+X right, +Y down, +Z forward**. Failed PnP estimates
retain the 2D detection, with both pose-related fields set to `null`.

## Validation

Build and run the environment and pipeline checks:

```sh
cmake --build build --target dependency_check pipeline_check --parallel 2
./build/detection/dependency_check
./build/detection/pipeline_check
```

After processing both videos with the commands above, validate the complete outputs:

```sh
./build/detection/pipeline_check \
  detection/results/small.jsonl \
  detection/results/large.jsonl
```

Without arguments, `pipeline_check` checks preprocessing, NMS, coordinate restoration,
known poses for all nine patterns, 3D transformations, failed/empty detection output,
and write failures. Given output files for the supplied clips, it additionally checks
360 records, sequential indices, timestamps, classes, physical edge lengths, and
independently recomputed reprojection RMS.

This version passed validation on macOS ARM64 with Apple Clang 21, OpenCV 4.14.0,
ONNX Runtime 1.31.0, yaml-cpp 0.9.0, and nlohmann-json 3.12.0:

| Video | Frames / JSONL records | Detections | Pose failures |
| --- | ---: | ---: | ---: |
| `small.mp4` | 360 | 397 | 0 |
| `large.mp4` | 360 | 151 | 0 |

All 16 headless Rust simulator tests also passed:

```sh
cargo test --manifest-path aiming/Cargo.toml --locked --no-default-features
```

These checks establish implementation and geometric consistency, not detection or
3D localization accuracy against manually labeled ground truth.

## Scope and limitations

- Model interface: float16 `[1,3,640,640]` input and float32 `[1,25200,22]` output.
- Timestamps follow the assignment's fixed 30 FPS contract. Other frame rates are not
  automatically supported. Video dimensions must match the calibration.
- Geometry is selected by pattern: `1` and `base_big` use large plates; other valid
  patterns use small plates.
- NMS is class-agnostic. PnP selects the valid candidate with all corners in front of
  the camera and the lowest reprojection error. No additional reprojection-error
  cutoff or temporal ambiguity resolution is applied.
- World coordinates, tracking, and hardware control are outside the implementation.
  The Rust student template's aiming strategy is unchanged.
- Decoded frame counts are checked when video metadata provides a total. Without that
  metadata, premature truncation cannot reliably be distinguished from normal EOF.

## Layout and references

```text
CMakeLists.txt          Root build entry point
detection/src/         Independent detection modules
detection/cmake/       Third-party dependency configuration
detection/tools/       Environment and pipeline checks
detection/assets/      Model, calibration, and two supplied clips
detection/results/     Generated local outputs, ignored by Git
aiming/                Original standalone Rust simulator and student template
```

- [Detection specification and asset sources](detection/README.en.md)
- [Environment and run guide, Chinese](detection/SETUP.md)
- [Step and module guide, Chinese](detection/STEPS.md)
- [Standalone Rust path](aiming/README.en.md)

The model, videos, and calibration are the supplied assignment assets. Their provenance
and reference implementation links remain in the detection specification.
