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

2026-11-02 读 dev1 上 `~/qt6.8-src/` 的源码，已经把排查范围从"三层都要查"
收窄到"只剩第 2 层"，而且找到了第 3 层的确切代码位置（不用再猜）。

### 第 3 层（RHI/Quick3D 怎么用这个 version）——已定位，不是根因，不用再查

`qtquick3d/src/runtimerender/qssgrendershadercache.cpp` 的
`initBakerForNonPersistentUse()`：

```cpp
case QRhi::OpenGLES2: {
    QSurfaceFormat format = ...;
    format = h->context->format();   // 读的就是下面第 2 层产出的那个值
    ...
    if (format.majorVersion() >= 3)
        outputs.append({ QShader::GlslShader, QShaderVersion(300, GlslEs) });
    else
        outputs.append({ QShader::GlslShader, QShaderVersion(100, GlslEs) });
```

这是**运行时**决定"这个 shader 要烤成哪个 GLSL 版本"的地方，且只烤一个
版本（不是多变体里挑）。它完全依据 `h->context->format().majorVersion()`
——如果这个值是对的（3），这里会正确生成 `QShaderVersion(300, GlslEs)`，
蒙皮用的 `textureSize()`/`texelFetch()`（定义在
`src/runtimerender/res/effectlib/skinanim.glsllib`，这段代码本身没问题，
没有另外的 ESSL100 fallback，也不需要）就能编译。**这一层只是如实转发
第 2 层给它的（错误的）版本号，不是独立的 bug，不用再花时间查这里**，
修复要往第 2 层去找。

### 第 2 层（Context 创建后的版本回填）——范围缩小到"为什么没触发"

`qtbase/src/gui/opengl/platform/egl/qeglplatformcontext.cpp` 里确实
**有**一个负责"创建后反查真实版本"的函数——`updateFormatFromGL()`
（约第 270 行）：

```cpp
void QEGLPlatformContext::updateFormatFromGL()
{
    ...
    if (ok) {
        const GLubyte *s = glGetString(GL_VERSION);
        if (s) {
            QByteArray version = QByteArray(reinterpret_cast<const char *>(s));
            int major, minor;
            if (QPlatformOpenGLContext::parseOpenGLVersion(version, major, minor)) {
                m_format.setMajorVersion(major);
                m_format.setMinorVersion(minor);
            }
        }
        ...
    }
}
```

手动验证过 `QPlatformOpenGLContext::parseOpenGLVersion()`（在
`qtbase/src/gui/kernel/qplatformopenglcontext.cpp`）对我们真机的真实
版本字符串 `"OpenGL ES 3.2 v1.g24p0-00eac0.4f825ed56fb37b32dde262046b053498"`
**能正确解析成 major=3, minor=2**（按空格分段取第 3 段 `"3.2"`，按 `.`
分段取整数，这段逻辑没问题）。**也就是说：如果 `updateFormatFromGL()`
真的在这个 context 上跑过，`m_format` 应该已经是 3.2 了，不会停在
2.0。**

这就把问题精确收窄成一个问题：**`updateFormatFromGL()` 到底有没有在
`QRhiGles2::create()` 创建的那个 context 上真正执行过，执行时
`glGetString(GL_VERSION)` 查到的又是什么？** 候选可能性（没有继续往下
查，需要加日志实测才能确定）：

- 没有被调用——`initialize()`（调用 `updateFormatFromGL()` 的入口）可能
  只在某些时机触发（代码注释里写"Cannot just call updateFormatFromGL()
  since it relies on virtuals. Defer it to initialize()"），`eglfs_kms`/
  `wayland-egl` 各自的 QPA 集成代码在创建 `QRhiGles2` 用的这个 context
  时，调用链路上可能没有触发到这个 `initialize()`。
- 被调用了，但 `eglMakeCurrent` 在临时 surface（pbuffer 或 surfaceless
  扩展）上失败或拿到了一个不同于真实窗口 context 的配置，导致这次
  `glGetString` 查到的不是真机那个 ES3.2 context 的版本（函数里本身有
  "Failed to make temporary surface current, format not updated" 的
  warning 分支，真机日志里没见过这条 warning，但没专门找过）。
- 这个 `QRhiGles2::create()` 用的 `ctx`（`qrhigles2.cpp` 里
  `new QOpenGLContext`）走的根本不是 `QEGLPlatformContext` 这条代码，
  而是另一条 QPA 插件专属的 context 实现（`eglfs_kms`/`wayland-egl`
  各自可能有自己的 `QPlatformOpenGLContext` 子类，只是复用了部分
  `qeglconvenience.cpp` 的辅助函数，不是整个继承 `QEGLPlatformContext`
  的创建+回填流程）——需要确认这两个 QPA 后端实际用的 context 类是不是
  真的是 `QEGLPlatformContext` 本身。

**下一步具体动作**：在 `updateFormatFromGL()`（约 273 行开始）里加
`qDebug() << "updateFormatFromGL called, ok=" << ok << "glGetString(GL_VERSION)=" << (const char*)glGetString(GL_VERSION);`
这样的打印，重新编译 `qtbase`（`cd ~/qt6.8-build/qtbase && ninja -j8 &&
ninja install`），真机跑一次看这行到底有没有出现、出现时内容是什么。
如果这行完全没出现过，说明是"没被调用"（上面第一种可能）——接下来要去
确认 `eglfs_kms`/`wayland-egl` 各自的 context 类是不是真的继承/调用了
这个基类函数。

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

## 结论（进行中）

- [x] RHI/Quick3D 决策层（第 3 层）：**已定位，确认不是独立 bug**，只是
  如实转发第 2 层的值，见上面 `qssgrendershadercache.cpp` 一节。
- [x] 顺带确认了`skinanim.glsllib` 的蒙皮代码本身没问题，不需要改——只要
  第 2 层修好，蒙皮会直接可以用（也因此否定了 `plan-
  custom-material-shader.md` 那条方向，不需要手写 shader）。
- [ ] 第 2 层（`updateFormatFromGL()`）实际有没有被调用、调用时查到
  什么——**这是现在唯一剩下的未知数**，见上面"下一步具体动作"，需要加
  日志实测，还没做。
- [ ] 第 1 层（EGL attrs 请求）：如果第 2 层确认"被调用但查到的就是错的
  2.0"，才需要往这层查；如果第 2 层确认"根本没被调用"，这层不是重点，
  重点是搞清楚 `eglfs_kms`/`wayland-egl` 的 context 创建路径。
- [ ] patch 后 `QSurfaceFormat` 是否正确报告 3.2
- [ ] patch 后蒙皮 shader 是否编译通过、渲染正确
- [ ] 帧率/内存验证
- [ ] 如果修复成功：是否启动"独立 Qt 构建仓库"的规划
