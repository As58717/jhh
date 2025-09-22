Easy360Equirect (UE 5.4 / 5.5 / 5.6) — SAFE v6.0
================================================
- RDG/Compute: Cubemap -> Equirect，支持 Mono / TB / SBS，分辨率可拉到 4K/8K。
- PNG 序列 + FFmpeg 复用，支持录制音频（Submix）与批量封装。
- 原生 NVENC：自动加载 nvEncodeAPI64.dll，支持 D3D11 / D3D12 零拷 H.264 / HEVC，并可附带音频封装。
- 立体输出新增单眼复制（Left / Right Only），方便生成“单眼立体”内容。
- Pipe 编码 UI 仍为保留项（示例代码未实现回读喂管）。

使用:
1) 解压到 YourProject/Plugins/Easy360Equirect/
2) 生成 VS 工程并编译 Development Editor (Win64)
3) 关卡里放置 AEqrCaptureActor；Window -> Easy360 打开面板；Start/Stop 录制。

NVENC 提示:
- 仅 Windows / NVIDIA GPU；驱动需携带 nvEncodeAPI64.dll（标准驱动自带）。
- 若需 HEVC 输出，勾选 `bNvencUseHEVC` 并确保播放端支持。FFmpeg 会自动加上 `-tag:v hvc1`。
- 录制单眼立体时，可在 StereoEyeSource 中选择 LeftOnly / RightOnly 以减少额外捕获开销。
