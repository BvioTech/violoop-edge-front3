# qt3d-spike

最小化验证：Qt Quick 3D（Wayland/EGLFS QPA + `RuntimeLoader` 运行时 glTF 导入）
能不能在 rk3576/Mali-G52 这台设备上跑 3D 动画。背景和设备已知坑见
`../../edge-front2/docs/display-stack-overview.md`（这台设备现有 Flutter
显示栈的事实汇总）。**真机已测完三轮，glTF 骨骼动画（蒙皮）最终确认
可行**，结论见 `docs/findings.md`：非蒙皮的 3D 动画可行（硬件加速、
kiosk-shell 兼容、帧率、短时内存都通过）；骨骼动画最早两轮跑不通是因为
一套从源码编译的 Qt 6.8.5 被自动配置成了桌面 OpenGL 而不是 GLES，用
`-opengl es2` 重新编译后修复，另外还顺带修了 `qtquicktimeline` 里一个
独立的关键帧 off-by-one bug，第三轮真机接显示器肉眼确认模型可见、动画
正常播放。

## 结构

```
qt3d-spike/
├── CMakeLists.txt          # Qt6 + Quick3D 可执行文件
├── main.cpp                # 最小 QGuiApplication + QQmlApplicationEngine
├── perf_monitor.h/.cpp     # 性能采样：fps/cpu/内存/gpu占用/温度，每秒写 CSV（见下方"性能记录"）
├── runtime_loader.qml      # View3D + RuntimeLoader 加载 glTF，显示 fps/渲染耗时
├── qml_runner.cpp          # 不用 CMake/资源打包，直接跑任意 .qml 文件（见文件头注释的 g++ 命令）
├── primitive_test.qml      # 非蒙皮的旋转 Cube 基线场景，配 qml_runner 用
├── scripts/
│   ├── install-deps.sh     # 目标设备上装 Qt6 + Quick3D 的完整依赖清单
│   ├── fetch-assets.sh     # 拉取 Khronos 官方带蒙皮动画的测试模型
│   ├── launch-under-weston.sh  # 复用 edge-front2 launcher 的 seat/weston 启动方式
│   ├── launch-eglfs.sh     # 不经 weston，Qt 直接拿 KMS（含 split-GPU 设备选择修复）
│   └── sample_rss.sh       # 采样进程 VmRSS，排查每帧内存泄漏
└── docs/
    ├── findings.md                       # 真机测试记录与结论（已有完整真机结果，不止是待办清单）
    ├── plan-custom-material-shader.md     # 方向 A：自定义材质手写 ES3 shader，绕开默认材质
    ├── plan-patch-qt-source.md            # 方向 B：直接改 Qt 源码定位修复版本协商 bug
    ├── plan-qt-fork-and-build.md          # 产品化计划：fork qtquicktimeline、打包、发私有源（已执行，见文件内"执行结果"）
    └── handoff-qt-build-glibc-mismatch.md # 交接：发布的 .deb 在目标设备上因 glibc/ICU 版本不匹配完全跑不起来，待修
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

**glTF 骨骼动画（蒙皮）最早两轮渲染不通过**——Quick3D 给蒙皮网格生成的
默认材质 shader 用到 `textureSize()`，需要 GLSL ES 3.00+，而 Qt 这两个
QPA 后端协商出的 context 在 Quick3D 看来一直是版本 2.0（即使硬件/驱动
实测是 ES3.2）。**不是 Qt 6.4.2 的版本 bug**（从源码编译的 Qt 6.8.5 原样
复现同一个失败），**也不是 Qt 的版本协商逻辑本身有 bug**——真正原因是
这套从源码编译的 qtbase 被自动配置成了桌面 OpenGL（`FEATURE_opengl_
desktop=ON`），而不是 GLES，导致版本查询链接到了错误的 GL 分发层。用
`-opengl es2` 重新编译后，蒙皮 shader 编译通过；修复后又暴露、并修复了
`qtquicktimeline` 里一个独立的关键帧 off-by-one bug；最后真机接显示器
肉眼确认骨骼动画渲染正确、动画在动。完整过程见 `docs/findings.md`
"2026-11-03（第三轮）"和 `docs/plan-patch-qt-source.md`。

## 性能记录

`qt3d_spike` 运行时每秒采样一次性能数据，写入 CSV（默认
`$TMPDIR/qt3d_spike_perf.csv`，可用 `QT3D_SPIKE_PERF_LOG` 环境变量改
路径）并同时打印到 stdout（`perf: ...` 前缀，会进已有的日志捕获流程）。
列：`timestamp,elapsed_s,fps,sync_ms,render_ms,cpu_pct,rss_kb,gpu_pct,
soc_temp_c,gpu_temp_c`。

- fps/sync/render：来自 Quick3D 的 `view3d.renderStats`（QML 侧读取）。
- cpu_pct：进程 CPU 占用，算法和 `top`/`ps` 一致（单核 100% 为满，
  多线程可能超过 100%），从 `/proc/self/stat` 的 utime+stime 算增量。
- rss_kb：`/proc/self/status` 的 `VmRSS`。
- gpu_pct/soc_temp_c/gpu_temp_c：读 dev1（rk3576/Mali-G52）专用
  的 sysfs 节点（`/sys/devices/platform/27800000.gpu/utilisation`、
  `/sys/class/thermal/thermal_zone{0,5}/temp`），换别的设备需要用
  `QT3D_SPIKE_GPU_UTIL_PATH`/`QT3D_SPIKE_GPU_TEMP_PATH`/
  `QT3D_SPIKE_SOC_TEMP_PATH` 环境变量覆盖，读不到就记 `-1`，不会崩。

## 已知限制

- Debian 的 `qt6-declarative-dev-tools` 没有打包通用 `qml` 脚本运行器，
  所以 `qt3d_spike` 是编译一个真正的 C++ 可执行文件（更贴近生产部署形态）；
  临时调试用 `qml_runner.cpp` 直接加载任意 `.qml` 文件。
- 内存只验证到约 5 分钟级别，长时间（小时级）运行是否有更慢的泄漏没有
  结论。
- 触摸坐标对齐完全没测（这轮场景没有任何交互元素，只验证了画面旋转本身
  是对的）。
- glTF 骨骼动画（蒙皮）已确认可行（见 `docs/findings.md`"2026-11-03
  （第三轮）"），但还没做正式的帧率/内存验证——目前只肉眼确认了渲染
  正确，没有量化数据。
