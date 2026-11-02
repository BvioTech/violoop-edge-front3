# 方向 B：直接改 Qt 源码，定位并修复 GLES 版本协商 bug

> 背景和问题全貌见 `findings.md`（尤其"glTF 骨骼动画限制"和
> "2026-11-02（第二轮）"两节）。这里只摘录这条方向需要的最小背景，
> 方便独立接手。

## 问题一句话复述

设备驱动（Mali-G52 + libmali r24p0）创建的 context 真实是 **OpenGL ES
3.2**（`glGetString(GL_VERSION)` 证实），但 Qt 自己记账给这个 context
标注的 `QSurfaceFormat.version()` 却是 **2.0**，不管应用层怎么请求
`QSurfaceFormat::setDefaultFormat()`（试过 `OpenGLES` + `(3,2)`，放在
`QGuiApplication` 构造前/后，两个 QPA 后端 `wayland-egl`/`eglfs_kms`，
四种组合结果一样）都不改变这个记账结果。Quick3D 的 shader
生成/选择逻辑依据这份（错误的）记账，给蒙皮网格挑了 ESSL100 的 shader
变体，而蒙皮 shader 需要的 `textureSize()`/`texelFetch()` 只有
ESSL300+ 才有，所以编译失败。已经验证过换 Qt 6.4.2 → 6.8.5（源码编译）
问题原样复现，**不是版本回归，是这条协商逻辑本身的问题**。

## 已有环境（dev1，不用重新搭）

- 源码：`~/qt6.8-src/{qtbase,qtshadertools,qtdeclarative,qtwayland,
  qtquicktimeline,qtquick3d}-everywhere-src-6.8.5/`
- 构建目录：`~/qt6.8-build/<module>/`（已配置好，`ninja -j8` 直接能增量
  编译）
- 安装 prefix：`~/qt6.8/`（已装好完整一套，`qt3d_spike` 测试程序已经能
  链接到这个 prefix 跑——参考 `/tmp/qt3d-spike/build-68/`，
  `CMAKE_PREFIX_PATH=/home/admin/qt6.8`）
- 复测方式：真机 EGLFS 路径，临时 systemd override 替换
  `violoop-edge-kiosk.service` 的 `ExecStart`（流程和收尾规范见
  `findings.md` 开头"环境"一节——**改完测完必须恢复**，这是生产设备）。

改完某个模块的源码后，只需要在对应的 `~/qt6.8-build/<module>/` 下
`ninja -j8 && ninja install`，不用重新跑 `qt-configure-module`（除非改了
构建配置本身）。

## 定位思路：先搞清楚"卡在 2.0"这件事发生在哪一层

这是最关键的未知数——目前只确认了"结果是错的"，没确认"错在哪一行"。
按从底层到高层排查：

1. **EGL context 创建层**（`qtbase/src/plugins/platforms/eglfs/` 和
   `qtbase/src/platformsupport/eglconvenience/`，尤其
   `qeglconvenience.cpp` / `qeglplatformcontext.cpp` 里构造
   `EGL_CONTEXT_MAJOR_VERSION`/`EGL_CONTEXT_MINOR_VERSION`（或老式的
   `EGL_CONTEXT_CLIENT_VERSION`）attribute 列表的代码）。
   - 排查问题：Qt 请求 context 时，有没有把应用层 `QSurfaceFormat`
     要求的 `(3,2)` 真正转成 EGL attribute 传给 `eglCreateContext`？
     还是请求阶段本身就固定传了个低版本（比如只用老式
     `EGL_CONTEXT_CLIENT_VERSION=2`，没走 `EGL_KHR_create_context`
     扩展协商高版本)？
   - 验证方法：在这几个创建 context 的函数里加 `qDebug()`/`fprintf`
     打印实际传给 `eglCreateContext` 的 attrib 数组，重新编译 `qtbase`
     （只需要重编 `qtbase`，这是这条链路最底层），真机跑一次看打印。

2. **Context 创建后的版本回填层**（`qtbase/src/gui/kernel/
   qopenglcontext.cpp`，找类似 `QOpenGLContext::create()` /
   处理 `format()` 更新的代码；也可能在 QPA 插件自己的
   `QPlatformOpenGLContext` 子类里）。
   - 排查问题：context 创建成功后，Qt 有没有反过来**查询**这个 context
     实际支持到什么版本（例如 `glGetIntegerv(GL_MAJOR_VERSION,
     ...)`/`GL_MINOR_VERSION`，或解析 `glGetString(GL_VERSION)`
     字符串），再用查到的真实版本去更新 `format()`？还是直接把"请求时
     的 format"原样当成"最终 format"用了，从头到尾没有拿真实值校正过？
   - 这是**最可能的根因位置**：如果 EGL 层其实已经拿到了 ES3.2
     context（驱动给的能力更高是正常行为，EGL/GLES driver 经常"你要 2
     我给你一个兼容 2 但其实是 3.x 能力"的 context），但 Qt
     从来没有在创建后反查一次真实版本，那 `format().version()`
     自然会一直停在"请求时"的旧值。

3. **RHI 层用这个 version 做什么决策**（`qtbase/src/gui/rhi/
   qrhigles2.cpp` 或 `qtbase/src/gui/rhi/qrhigles2_p.h`，搜
   `isES`、GLSL 版本选择、`QRhiGles2InitParams` 相关逻辑；以及
   `qtshadertools`/`qtquick3d` 里实际按 RHI backend 报告的能力选
   shader 变体的代码，搜 `textureSize`、`ESSL`、`shaderVersion` 之类
   关键词缩小范围）。
   - 排查问题：即使第 2 层修好了，拿到了正确的 `3.2`，RHI/Quick3D
     这一层是否真的会用它来挑 ESSL300 变体？还是这里也有一处独立的
     "固定假设 ES2 profile"的硬编码？

**建议第一个动作**：在 (2) 这一层加日志，因为最可能是"请求值 ==
最终记账值，中间从没查询过真实值"这种模式。如果 (2) 改完发现
`format().version()` 变成了 `3.2` 但 shader 挑选还是不对，再往 (3)
查。如果 (2) 改完发现 EGL attrib 本身请求就没对（(1) 的问题），先回头
修 (1)。

## 验证流程

1. 改动代码后，`cd ~/qt6.8-build/qtbase && ninja -j8 && ninja install`
   （如果改的是别的模块，对应切换目录）。
2. 重新编译 `/tmp/qt3d-spike/build-68/`（`cd build-68 && ninja`）。
3. 走真机 EGLFS 临时 override 流程（见上面"已有环境"一节），用
   `CesiumMan.glb` 复测，看 `qt.rhi.general` 日志里
   `Created OpenGL context QSurfaceFormat(version ...)` 这一行是否变成
   了 `3.2`，以及蒙皮 shader 是否不再报 `textureSize is not supported
   in ESSL 100`。
4. **测完立刻按 `findings.md` 开头的收尾流程恢复设备**：删除
   `/etc/systemd/system/violoop-edge-kiosk.service.d/`、
   `daemon-reload`、`restart violoop-edge-kiosk.service`，确认跑的是
   `/opt/violoop-edge-kiosk/violoop_edge_kiosk`。

## 如果修复成功：后续怎么维护这个 patch

这会立刻把之前讨论过、当时**明确搁置**的问题变成现实问题：要不要另外
fork 一个仓库专门构建"我们自己的 Qt"。如果这条路修好了：

- 这个 patch 需要长期维护（至少到上游真的修了、或者我们确认不需要再
  跟进 Qt 版本更新为止）。
- 建议把 patch 本身（diff 或 cherry-pick 的 commit）、补丁适用的 Qt
  版本号、编译脚本（这次摸出来的模块顺序 + `qt-configure-module`
  参数）整理进一个独立仓库（例如 `violoop-qt-build`），而不是留在
  `edge-front3` 里——`edge-front3` 该放的是产品代码，不是 Qt 构建基础
  设施。这部分等这条方向真的跑通了再具体规划，不要提前投入。

## 结论（待填）

- [ ] 定位到问题具体在哪一层（EGL 创建 / 版本回填 / RHI 决策）
- [ ] patch 后 `QSurfaceFormat` 是否正确报告 3.2
- [ ] patch 后蒙皮 shader 是否编译通过、渲染正确
- [ ] 帧率/内存验证
- [ ] 如果修复成功：是否启动"独立 Qt 构建仓库"的规划
