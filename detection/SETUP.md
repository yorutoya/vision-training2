# C++ 检测路线环境

本配置覆盖检测路线的全部依赖。`aiming` 是独立的 Rust 路线。

| 依赖 | 用途 |
| --- | --- |
| OpenCV 4 | 视频读取、图像预处理、可视化、NMS、PnP |
| ONNX Runtime | 在 CPU 上运行 `0526.onnx` |
| yaml-cpp | 读取 `calibration.yaml` |
| nlohmann-json | 写出 JSON Lines |

## macOS 安装

已有 Homebrew 时，缺少的依赖可通过以下命令安装：

```sh
brew install opencv@4 onnxruntime yaml-cpp nlohmann-json
```

CLion 自带 CMake、Ninja，编译器使用 Xcode Command Line Tools 的 Apple Clang。
CMake 配置会查找 Homebrew 的安装路径，无需手动给每个头文件添加搜索路径。

## 在 CLion 中使用

1. 打开仓库根目录或 `detection` 目录，加载对应的 `CMakeLists.txt`。
2. 工具链使用 Apple Clang，CMake 和 Ninja 使用 CLion 自带版本。
3. 选择 `dependency_check`，点击运行；不需要填写程序参数。

检查程序会读取标定文件，完整解码两段视频，并以全零 float16 输入运行一次模型。
成功时返回退出码 0，显示两段视频各 360 帧、`model_cpu_inference: OK` 和输出形状
`[1, 25200, 22]`。这只是环境验证，不代表完成了装甲板检测。

构建配置分为三层：

- 根目录 `CMakeLists.txt`：项目入口，加载检测子目录。
- `detection/CMakeLists.txt`：维护主程序和检查工具的源文件列表。
- `detection/cmake/Dependencies.cmake`：查找 Homebrew 和第三方库，统一提供依赖。

`vision_detection` 是主程序，各步骤分别实现在独立 `.cpp/.h` 文件。
完整编号和阅读顺序见 [STEPS.md](STEPS.md)。`main.cpp` 只负责初始化、按顺序调用和收尾。

在 CLion 将工作目录设为 `detection`，Program arguments 填写：

```text
--model assets/0526.onnx --video assets/small.mp4 --calibration assets/calibration.yaml --output small.jsonl
```

处理另一段时将视频和输出分别改为 `assets/large.mp4`、`large.jsonl`。
仍支持旧参数 `assets/small.mp4`，默认输出到工作目录的 `small.jsonl`。
输出文件存在时会覆盖；需要保留旧结果时指定新路径。输出父目录需事先创建。
程序拒绝将模型、视频、标定本身作为输出路径，包括同一文件的链接。

JSONL 每帧一行，包含原图角点、四个相机坐标系角点及重投影 RMS。
PnP 失败仍保留检测，两个位姿字段为 `null`；空检测帧也有对应行。
控制台仅显示汇总，详细数据在输出文件中。运行失败返回非零退出码，
中途错误可能留下部分结果，不应把它当成完整视频输出。

`dependency_check` 检查环境；`pipeline_check` 检查业务计算，两者均不参与默认构建。
新增业务模块应放入 `VISION_MODULE_SOURCES`，两个业务目标会使用同一份实现。
不要把带独立 `main` 的检查工具加入主程序源文件列表。

## 命令行构建

如果 `cmake` 已在 PATH 中，在仓库根目录执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
cmake --build build --target dependency_check
./build/detection/dependency_check
```

上面的 `dependency_check` 默认使用项目 assets，也可将其他素材目录作为第一个参数。

运行完整检测与结果验证（仓库根目录）：

```sh
mkdir -p detection/results
./build/detection/vision_detection --model detection/assets/0526.onnx --video detection/assets/small.mp4 --calibration detection/assets/calibration.yaml --output detection/results/small.jsonl
./build/detection/vision_detection --model detection/assets/0526.onnx --video detection/assets/large.mp4 --calibration detection/assets/calibration.yaml --output detection/results/large.jsonl
cmake --build build --target pipeline_check --parallel 2
./build/detection/pipeline_check detection/results/small.jsonl detection/results/large.jsonl
```

`pipeline_check` 无参数时运行合成测试；传入给定素材的输出时，额外检查每段 360 行、
帧号、时间戳、字段、类别、角点边长和独立重投影 RMS。
默认置信度/NMS 阈值均为 0.5，沿用题目建议，没有针对视频调整。
可用 `--confidence` 和 `--nms` 改为 [0,1] 范围内的有限数值。
标定分辨率必须匹配输入视频；时间戳按本作业规定的 30 FPS 计算。
