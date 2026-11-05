# vision-detection3

[中文](README.md) · [English](README.en.md)

基于 C++17、OpenCV 和 ONNX Runtime 的离线装甲板检测程序。读取视频后，完成
模型推理、角点解码、重复检测抑制和 PnP 位姿估计，按帧输出二维角点、相机坐标系下
的三维角点及重投影误差。

本项目以 `vision-training2` 的实现为基础，移除源码注释并保留独立模块结构。
已完成的是 `detection/` 检测路线；`aiming/` 保留原有 Rust 仿真与学生模板，
不代表已完成预测瞄准策略，也不参与 CMake 检测程序的构建。

## 功能与流程

| 步骤 | 模块 | 功能 |
| --- | --- | --- |
| 0 | `cli`、`calibration` | 解析路径和阈值，读取相机标定 |
| 1 | `video_read` | 按顺序解码视频帧 |
| 2.1 | `preprocess` | Letterbox、RGB、NCHW、归一化与 float16 转换 |
| 2.2 | `inference` | 复用 ONNX 会话完成逐帧推理 |
| 3 | `postprocess` | 解码四角点、置信度及类别，执行矩形 NMS |
| 4 | `coordinate_restore` | 撤销缩放与补边，恢复原图像素坐标 |
| 5 | `armor_geometry`、`pose` | 根据图案选择几何尺寸，以 IPPE 求解位姿 |
| 6.1 | `camera_transform` | 对四个物理角点分别执行 `R × P + t` |
| 6.2 | `json_output` | 每帧写出一条 JSON Lines 记录 |

所有业务模块分别存放在 `.h/.cpp` 文件中，`main.cpp` 负责资源初始化、调用顺序和错误处理。

## 依赖与构建

需要 CMake 3.20+、支持 C++17 的编译器，以及：

- OpenCV 4：core、imgproc、imgcodecs、videoio、highgui、calib3d、dnn。
- ONNX Runtime、yaml-cpp、nlohmann-json。

macOS 使用 Homebrew 时：

```sh
brew install cmake opencv@4 onnxruntime yaml-cpp nlohmann-json
```

在项目根目录构建：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target vision_detection --parallel 2
```

CMake 会查找常见 Homebrew 安装位置。其他安装布局可通过 `CMAKE_PREFIX_PATH`
或各依赖的包配置目录指定搜索路径。本程序使用 CPU，不要求 CUDA 或真实相机。

## 运行

以下命令均从项目根目录执行：

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

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--video` | 输入视频；必填，也可使用单个位置参数 | 无 |
| `--model` | ONNX 模型路径 | `assets/0526.onnx` |
| `--calibration` | 标定 YAML 路径 | `assets/calibration.yaml` |
| `--output` | 输出 JSONL 路径 | 工作目录下的 `<视频文件主名>.jsonl` |
| `--confidence` | 置信度阈值，范围 `[0,1]`，严格大于才保留 | `0.5` |
| `--nms` | NMS 的 IoU 阈值，范围 `[0,1]` | `0.5` |
| `--help` / `-h` | 显示帮助，不执行推理 | — |

所有相对路径均以**进程工作目录**为基准。默认模型、标定路径适用于从 `detection/`
目录运行；从项目根目录运行时请使用上方的显式参数。

输出父目录需要事先创建。开始写入时会覆盖已有输出，运行中途失败可能留下部分帧；
应以退出码 `0` 判断正常完成。程序会拒绝将模型、视频或标定文件本身作为输出路径。

## 在 CLion 中运行

打开根目录 `CMakeLists.txt`，构建并选择目标 `vision_detection`。工作目录设为项目根目录，
Program arguments 可填写：

```text
--model detection/assets/0526.onnx --video detection/assets/small.mp4 --calibration detection/assets/calibration.yaml --output small.jsonl
```

也可直接打开 `detection/CMakeLists.txt`，将工作目录设为 `detection/`，使用短参数
`assets/small.mp4`；此时默认输出为该工作目录下的 `small.jsonl`。

## 输出格式

输出 UTF-8 JSON Lines，每行一个完整 JSON 对象，不使用外层数组。每帧都有记录，
包括没有检测的帧。帧号从 `0` 开始，`timestamp_s = frame_index / 30.0`。

```json
{"frame_index":0,"timestamp_s":0.0,"detections":[]}
```

每个检测包含：

| 字段 | 内容 |
| --- | --- |
| `color` | `blue`、`red`、`neutral` 或 `purple` |
| `pattern` | `sentry`、`1`～`5`、`outpost`、`base_small` 或 `base_big`，均为字符串 |
| `confidence` | `sigmoid(objectness)`，不乘类别分数 |
| `corners_px` | 原图坐标中的四个 `[x,y]`，单位像素 |
| `corners_camera_m` | 相机坐标系中的四个 `[x,y,z]`，单位米；位姿失败时为 `null` |
| `reprojection_error_px` | 四点重投影欧氏误差的 RMS；位姿失败时为 `null` |

角点顺序固定为装甲板自身方向的**左上、左下、右下、右上**。相机坐标系为
**+X 向右、+Y 向下、+Z 向前**。PnP 失败不会删除二维检测，两个位姿字段同时置为 `null`。

## 验证

检查环境并构建业务验证工具：

```sh
cmake --build build --target dependency_check pipeline_check --parallel 2
./build/detection/dependency_check
./build/detection/pipeline_check
```

先运行上面的两条视频处理命令，再验证完整输出：

```sh
./build/detection/pipeline_check \
  detection/results/small.jsonl \
  detection/results/large.jsonl
```

`pipeline_check` 无参数时验证预处理、NMS、坐标恢复、九种图案的已知姿态、三维角点转换、
失败/空检测输出和写入错误；传入给定视频的结果时，还会核对 360 行、连续帧号、时间戳、
类别、角点边长及独立重投影 RMS。

本版本在 macOS ARM64、Apple Clang 21、OpenCV 4.14.0、ONNX Runtime 1.31.0、
yaml-cpp 0.9.0、nlohmann-json 3.12.0 环境下验证通过：

| 视频 | 帧数 / JSONL 行数 | 检测数 | 位姿失败数 |
| --- | ---: | ---: | ---: |
| `small.mp4` | 360 | 397 | 0 |
| `large.mp4` | 360 | 151 | 0 |

Rust 仿真路线的无界面测试也通过，共 16 项：

```sh
cargo test --manifest-path aiming/Cargo.toml --locked --no-default-features
```

这些结果验证实现和几何一致性，不代表基于人工真值的检测或三维定位精度评估。

## 范围与限制

- 使用给定模型：输入 float16 `[1,3,640,640]`，输出 float32 `[1,25200,22]`。
- 时间戳固定按作业的 30 FPS 计算；程序不自动适配其他帧率。视频分辨率必须匹配标定。
- 大小装甲板由图案决定：`1` 和 `base_big` 使用大装甲板，其他有效图案使用小装甲板。
- NMS 跨类别执行；PnP 选择全部角点在相机前方且重投影误差最小的有效候选。
  当前没有额外重投影误差淘汰阈值或跨帧消歧。
- 未实现世界坐标、跨帧跟踪或硬件控制；未修改 Rust 学生模板的瞄准策略。
- 能获取视频总帧数时会检查解码完整性；总帧数未知时，无法可靠区分正常结束和截断。

## 目录与参考

```text
CMakeLists.txt          根目录构建入口
detection/src/         检测路线的独立业务模块
detection/cmake/       第三方依赖配置
detection/tools/       环境与业务验证工具
detection/assets/      模型、标定与两段视频
detection/results/     本地生成结果，Git 忽略
aiming/                原有独立 Rust 仿真与学生模板
```

- [检测任务规格与素材来源](detection/README.md)
- [环境和运行说明](detection/SETUP.md)
- [步骤与模块说明](detection/STEPS.md)
- [独立 Rust 路线](aiming/README.md)

模型、视频和标定沿用原作业提供的素材；其来源与参考实现链接保留在检测任务规格中。
