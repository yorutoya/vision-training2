# 检测程序步骤与阅读顺序

沿用题目 README 的 1～6 步编号。步骤 0 是初始化，步骤 7 是收尾；
同一步中的小任务使用小数编号。每个模块由 `.h` 声明接口、`.cpp` 实现，
通过 CMake 链接到 `vision_detection`，不是把算法全写进一个 `main.cpp`。

| 编号 | 模块 | 输入 → 输出 | 关键点 |
| --- | --- | --- | --- |
| 0.1 | `cli.h/.cpp` | argv → ProgramOptions | 路径、阈值、帮助、输入文件保护 |
| 0.2 | `calibration.h/.cpp` | YAML → CameraCalibration | 原图内参、畸变、分辨率 |
| 0.3～0.4 | `main.cpp` 初始化段 | 配置 → 会话、视频、输出流 | 每个资源只初始化一次 |
| 1 | `video_read.h/.cpp` | VideoCapture → BGR 帧 | 顺序读取，失败时结束循环 |
| 2.1 | `preprocess.h/.cpp` | BGR 原帧 → PreprocessedImage | letterbox、RGB、CHW、归一化、float16 |
| 2.2 | `inference.h/.cpp` | 模型会话和输入 → 原始预测 | 一帧一次，输出 25200×22 float32 |
| 3 | `postprocess.h/.cpp` | 原始预测 → Detection 列表 | sigmoid、类别 argmax、矩形 NMS |
| 4 | `coordinate_restore.h/.cpp` | 模型角点 → 原图角点 | (点−补边)/scale，不改变角点顺序 |
| 5.1 | `armor_geometry.h/.cpp` | 图案编号 → 物理角点 | 1/base_big 为大装甲板；与 6.1 共用 |
| 5.2～5.3 | `pose.h/.cpp` | 原图角点、几何、标定 → 可选位姿 | IPPE 候选、正深度、最小重投影 RMS |
| 6.1 | `camera_transform.h/.cpp` | 几何、位姿 → 可选 CameraPose | 每个角点应用 R×Po+t，单位米 |
| 6.2 | `json_output.h/.cpp` | 一帧 DetectionResult → 一行 JSON | 空帧不遗漏，失败位姿为 null |
| 7 | `main.cpp` 收尾段 | 输出流、帧计数 → 退出状态 | 检查写盘与解码帧数，打印汇总 |

## 先理解数据怎么变化

1. `PreprocessedImage` 拥有模型输入内存，并记录 scale/padding。
2. `Detection` 保存置信度、类别和二维角点。步骤 3 返回模型坐标，步骤 4 原地恢复原图坐标。
3. `PoseResult` 保存 rvec/tvec 和四点 RMS；`optional` 为空表示 PnP 失败。
4. `CameraPose` 保存四个相机坐标系角点。它和原 Detection 组合成 `DetectionResult`。
5. 输出层将所有 DetectionResult 放进本帧 `detections` 数组，每帧写一行。

`rvec` 是轴角向量，不是欧拉角；先通过 Rodrigues 转为旋转矩阵 R。
`tvec` 是装甲板中心，只有 `R×物理角点+tvec` 才是对应角点的位置。
全部二维/三维角点保持左上、左下、右下、右上顺序，不能按图像 x/y 再排序。

## 失败怎样传递

- 文件、参数、标定、推理或写盘错误：异常传到 main，报告原因并返回 1。
- 单个检测位姿失败：返回空 optional，保留该检测，两个位姿字段同时为 null。
- 本帧没有检测：写入空 detections 数组，仍推进帧号。
- 正常完成：关闭输出并检查状态，返回 0。中途失败的文件可能只包含部分帧。

## 怎样验证

运行方式与 CMake 目标见 [SETUP.md](SETUP.md)。`pipeline_check` 包含：

1. 已知像素检查 RGB/NCHW、归一化及灰色补边。
2. 人工预测检查 objectness 下标、类别、重复候选和末行候选。
3. 已知模型点检查原图坐标恢复。
4. 九种图案的已知姿态检查几何尺寸、PnP 与完整 R×Po+t。
5. 成功/失败/空检测 JSON 输出和写入错误检查。
6. 真实视频结果逐行检查，并将输出三维角点独立投影回图像，核对 RMS。

当前采用默认 0.5 置信度和 0.5 NMS 阈值。PnP 选最小有效误差候选，
没有额外误差淘汰阈值或跨帧消歧；误差小不能单独证明类别或真实距离正确。
