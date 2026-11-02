# qt3d-spike

最小化验证：Qt Quick 3D（Wayland/EGLFS QPA + `RuntimeLoader` 运行时 glTF 导入）
能不能在 rk3576/Mali-G52 这台设备上跑 3D 动画。背景和设备已知坑见
`../../edge-front2/docs/display-stack-overview.md`（这台设备现有 Flutter
显示栈的事实汇总）。**真机已测完一轮**，结论见 `docs/findings.md`：非蒙皮的
3D 动画可行（硬件加速、kiosk-shell 兼容、帧率、短时内存都通过），glTF
骨骼动画目前跑不通（Qt 6.4.2 的 GLES 版本协商限制）。

## 结构

```
qt3d-spike/
├── CMakeLists.txt          # Qt6 + Quick3D 可执行文件
├── main.cpp                # 最小 QGuiApplication + QQmlApplicationEngine
├── runtime_loader.qml      # View3D + RuntimeLoader 加载 glTF，显示 fps/渲染耗时
├── qml_runner.cpp          # 不用 CMake/资源打包，直接跑任意 .qml 文件（见文件头注释的 g++ 命令）
├── primitive_test.qml      # 非蒙皮的旋转 Cube 基线场景，配 qml_runner 用
├── scripts/
│   ├── install-deps.sh     # 目标设备上装 Qt6 + Quick3D 的完整依赖清单
│   ├── fetch-assets.sh     # 拉取 Khronos 官方带蒙皮动画的测试模型
│   ├── launch-under-weston.sh  # 复用 edge-front2 launcher 的 seat/weston 启动方式
│   ├── launch-eglfs.sh     # 不经 weston，Qt 直接拿 KMS（含 split-GPU 设备选择修复）
│   └── sample_rss.sh       # 采样进程 VmRSS，排查每帧内存泄漏
└── docs/findings.md        # 真机测试记录与结论（已有完整真机结果，不止是待办清单）
```

`assets/`（`fetch-assets.sh` 的输出目录）不入库，是下载下来的 Khronos 官方
二进制测试模型。

## 在目标设备上跑

```bash
# 1. 装依赖（Qt6.4.2 在 Debian bookworm 官方源，不需要额外 repo）
./scripts/install-deps.sh

# 2. 拉测试模型
./scripts/fetch-assets.sh

# 3. 编译
cmake -S . -B build -G Ninja
cmake --build build

# 4. 离屏快速验证 QML/RuntimeLoader 没有基础错误（不需要显示/GPU）
QT_QPA_PLATFORM=offscreen timeout 5 ./build/qt3d_spike assets/CesiumMan.glb

# 5. 真机 GPU 测试：两条路径都已经跑通过，见 scripts/launch-under-weston.sh
#    （走现有 weston kiosk-shell）和 scripts/launch-eglfs.sh（Qt 直接拿
#    KMS，不经 weston）顶部注释。这一步会临时顶替
#    violoop-edge-kiosk.service 的 ExecStart，需要在目标设备上对生产 kiosk
#    服务做改动——必须先取得明确授权，测完要换回去（本仓库这轮测试已经
#    换回去了，结果见 docs/findings.md）。
```

## 结论（详见 docs/findings.md）

真机（dev1，rk3576/Mali-G52）已经测过 Wayland 和 EGLFS 两条 QPA 路径：
硬件加速渲染、weston kiosk-shell 全屏兼容、帧率（稳定 59-60fps，匹配
面板 60Hz）、应用侧 90° 旋转 + 居中（真机截图确认）都通过了。内存：
20 秒和追加的约 5 分钟连续观察里，非蒙皮场景的 `VmRSS` 基本不涨（5 分钟
内净变化为 0），看起来明显不会复现 Flutter/GTK embedder 那条每帧 GL
对象泄漏（基线约 20MB/min，见 edge-front2 文档 §5），但 5 分钟仍然只是
中期观察，不能排除小时级的更慢泄漏。

**但 glTF 骨骼动画（蒙皮）渲染不通过**——Quick3D 给蒙皮网格生成的默认
材质 shader 用到 `textureSize()`，需要 GLSL ES 3.00+，而 Qt 6.4.2 这两个
QPA 后端协商出的 context 在 Quick3D 看来一直是版本 2.0（即使硬件/驱动
实测是 ES3.2）。非蒙皮的 3D 动画（旋转、变换、程序化）不受影响。

## 已知限制

- Debian 的 `qt6-declarative-dev-tools` 没有打包通用 `qml` 脚本运行器，
  所以 `qt3d_spike` 是编译一个真正的 C++ 可执行文件（更贴近生产部署形态）；
  临时调试用 `qml_runner.cpp` 直接加载任意 `.qml` 文件。
- 内存只验证到约 5 分钟级别，长时间（小时级）运行是否有更慢的泄漏没有
  结论。
- 触摸坐标对齐完全没测（这轮场景没有任何交互元素，只验证了画面旋转本身
  是对的）。
- glTF 骨骼动画这条限制还没找到绕过办法（候选方向见 `docs/findings.md`
  "未覆盖"一节：换更新的 Qt 版本、少骨骼数、自定义材质手写 shader）。
