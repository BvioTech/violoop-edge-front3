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

### 第 2 层——真正的根因找到了：qtbase 配置成了桌面 OpenGL，不是 GLES

2026-11-03 在 `updateFormatFromGL()` 里加 `qDebug()` 实测（dev1，真机
EGLFS），逐步定位到了确切原因，**不是猜测，是实测确认的**：

1. `updateFormatFromGL()` **确实被调用了**（`QEGLPlatformContext::
   initialize()` → `QOpenGLContext::create()` 链路正常触发，每次创建新
   context 都会走到）。
2. `eglMakeCurrent()` **确实成功**（`ok=true`，`eglGetError()=
   EGL_SUCCESS`，`eglGetCurrentContext()` 确认当前 context 就是刚创建的
   那个，renderableType 也确认是 `OpenGLES`）。
3. 但紧接着调用的 `glGetString(GL_VERSION)` **返回 null**（`glGetError()`
   同时报 `GL_NO_ERROR`）——即"EGL 认为 make-current 成功了"，但
   "glGetString 却查不到任何东西"。换成强制走真实 pbuffer（而不是
   surfaceless 临时 surface）结果完全一样，排除了"surfaceless
   context 在这块驱动上有问题"这个猜测。
4. 一路查到底层链接：

   ```
   $ ldd libQt6Gui.so.6 | grep -i "GL\|EGL"
       libEGL.so.1 => /usr/lib/aarch64-linux-gnu/mali/libEGL.so.1   # 对的，真的是 Mali 的 EGL
       libOpenGL.so.0 => /lib/aarch64-linux-gnu/libOpenGL.so.0      # 问题在这
   ```

   `readelf -d` 确认 `libQt6Gui.so.6` NEEDED 里有 `libGLX.so.0` +
   `libOpenGL.so.0`——这是 **GLVND**（GL Vendor-Neutral Dispatch）的桌面
   OpenGL 分发层，不是 Mali 的 `libGLESv2.so`。

5. 根因：`~/qt6.8-build/qtbase/CMakeCache.txt` 里
   `FEATURE_opengl_desktop=ON`、`FEATURE_opengles2=OFF`——**这套 qtbase
   build 当时被 configure 自动探测成了"桌面 OpenGL"模式**（大概率因为
   dev1 这台开发机上装了 Mesa 的 `libGL`/`libGLX`/`libOpenGL` dev
   包），不是 GLES 模式。于是 `qeglplatformcontext.cpp` 里直接调用的
   `glGetString()`/`glGetIntegerv()` 这些裸 C 符号，链接的是 **GLVND 的
   分发层**，而 GLVND 自己维护一套"当前 context"记账，只认它知道的
   vendor（通常是 GLX/桌面驱动），**完全不知道 Mali 的 `libEGL`
   刚刚通过 `eglMakeCurrent()` 设的那个 context**——所以从 GLVND
   分发层的视角看"没有当前 context"，`glGetString` 直接返回
   null、`glGetError()` 也查不到真实错误（没有真实 context 可查）。

   同时能解释"为什么非蒙皮渲染完全正常"：Quick3D/RHI 真正用来画画的 GL
   调用，是通过 `eglGetProcAddress()` **动态**解析函数指针
   （`QOpenGLFunctions`/`QOpenGLExtraFunctions` 的标准做法），这条路径
   始终拿到的是 Mali 真实驱动的函数——只有 `qeglplatformcontext.cpp`
   这几处直接写死调用裸 C 符号的地方，才会被静态链接坑到 GLVND 上。

**结论**：这不是"EGL 版本协商逻辑有 bug"，是**这套 Qt 构建从一开始就配置
错了底层 OpenGL 后端**（桌面 OpenGL/GLVND，而不是 GLES）。修复不需要改
Qt 任何一行逆向工程出来的业务逻辑代码，只需要**用正确的 configure 参数
重新编译 qtbase**：

```bash
cd ~/qt6.8-build/qtbase-es2   # 全新 build 目录，避免旧 cache 残留
~/qt6.8-src/qtbase-everywhere-src-6.8.5/configure \
    -prefix /home/admin/qt6.8 -release -opengl es2 -no-feature-xcb \
    -nomake examples -nomake tests
cmake --build . --parallel 8
cmake --install .
```

配置后确认 `config.summary` 里 `Desktop OpenGL: no` / `OpenGL ES 3.2:
yes`，`CMakeCache.txt` 里 `FEATURE_opengl_desktop=OFF` /
`FEATURE_opengles2=ON`。2026-11-03 已经跑完 configure 并确认这两个
开关是对的，完整编译正在进行（见下面"结论"一节的进度）。

> 调试用的 `qDebug()` 日志（`[dbgpatch]` 前缀）还留在
> `qeglplatformcontext.cpp` 里，等这条修复验证通过后要清理掉再定稿。

### 第 2 层（旧记录，已被上面的实测结论取代，留档）

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
- [x] 第 2 层（`updateFormatFromGL()`）实际有没有被调用、调用时查到
  什么——**真正根因已找到，不是版本协商逻辑本身的 bug**：这套 qtbase
  build 当初被 configure 成了桌面 OpenGL（`FEATURE_opengl_desktop=ON`，
  `FEATURE_opengles2=OFF`），导致 `glGetString()` 这类裸 C 符号链接到
  GLVND 桌面分发层而不是 Mali 的 `libGLESv2.so`，GLVND 不认 Mali EGL
  设的 context，所以查不到真实版本。详见上面"第 2 层——真正的根因找到
  了"一节。**不需要改任何 Qt 业务逻辑代码，只需要用 `-opengl es2`
  重新 configure + 编译 qtbase。**
- [x] 第 1 层（EGL attrs 请求）：不再是重点——根因在构建配置层，不在
  EGL context 创建的属性列表上，这层不需要再查。
- [x] 用 `-opengl es2` 重新编译的 qtbase 装上后，`QSurfaceFormat` 是否
  正确报告 3.2——**是，已真机验证**。2026-11-03 完整重新编译了
  qtbase（GLES 配置）+ 其余五个模块（qtshadertools/qtdeclarative/
  qtwayland/qtquicktimeline/qtquick3d，全部针对新 qtbase 重新链接），
  装到同一个 `/home/admin/qt6.8` prefix。真机复测日志：
  `qt.rhi.general: Created OpenGL context QSurfaceFormat(version 3.2,
  ...)`，`updateFormatFromGL` 里 `glGetString(GL_VERSION)` 也正确拿到
  了真实驱动字符串并解析成 major=3/minor=2——不再是 null。
- [x] 蒙皮 shader 是否编译通过——**是**。同一次真机复测，完整日志里
  **没有再出现** `Failed to compile vertex shader: textureSize is not
  supported in ESSL 100`，也没有再出现 `WARN: Model has non-integer
  type indices for skinning`，`RuntimeLoader status: 1 errorString:
  Success!`（加载成功）。**ESSL100/GLES 版本协商这个根因 bug 确认修复。**
- [x] 渲染正确性（骨骼动画部分）——**发现并修复了第二个、独立的 Qt
  bug**。蒙皮 shader 编译通过后，运行时疯狂刷日志（几秒内 1 万多条）：
  ```
  Could not find any constructor for value type QQuickVector3DValueType to call with value QVariant(Invalid)
  Cannot set property "position"
  ```
  定位到 `qtquicktimeline-everywhere-src-6.8.5/src/timeline/
  qquickkeyframe.cpp` 的 `QQuickKeyframeGroupPrivate::loadKeyframes()`
  （被 `QQuickKeyframeGroup::setProperty()` 间接触发）——**这是一个独立
  于 ES3/GLES 问题的、真实存在于 Qt 6.8.5 本身的 off-by-one 循环边界
  bug**，和我们这轮的构建配置无关：

  ```cpp
  // 原来的代码（有 bug）：
  for (qsizetype i = 0; i < arraySize - frameSize; i += frameSize) { ... }
  ```

  `arraySize = N * frameSize`（N 个关键帧，每个占 `frameSize` 个数组
  槽位，Vec3/position 类型 `frameSize=5`：time + easing + x/y/z，见
  `validFrameSize()`）。按这个循环条件算，**永远只处理 N-1
  个关键帧，漏掉最后一个**——对大多数通道这只是漏掉最后一帧（影响不大，
  可能有轻微的动画收尾瑕疵），但**对恰好只有 1 个关键帧的通道
  （`arraySize == frameSize`），循环条件变成 `i < 0`，一次都不执行**，
  这个属性的 `keyframes` 列表永远是空的——`QQuickKeyframeGroup::
  evaluate()` 看到 `sortedKeyframes.isEmpty()` 就直接 `return
  QVariant()`（无效值），于是每一帧都尝试把无效值写进 `position`
  属性，刷出海量警告。CesiumMan.glb 的骨骼里显然至少有一个关节的
  `position`（translation）通道只有 1 个关键帧（静止不动的骨骼，glTF
  导出器仍然写了一个显式的单帧通道，而不是干脆不写这个通道）。

  **这是 ES3 bug 修好之后才有机会暴露出来的**——之前蒙皮 shader
  编译不过，这段运行时动画绑定代码虽然仍会执行（和 GPU 渲染是否成功
  无关），但当时没人专门盯着这类次要日志去找；现在蒙皮能跑了，这个
  问题才被我们注意到并查清楚。

  **修复**：把循环条件改成不漏最后一个元素：

  ```cpp
  for (qsizetype i = 0; i + frameSize <= arraySize; i += frameSize) { ... }
  ```

  重新编译 `qtquicktimeline`（`cd ~/qt6.8-build/qtquicktimeline &&
  ninja -j8 && ninja install`），真机复测：**`Could not find any
  constructor`/`Cannot set property` 这组警告完全消失（0 条）**，
  `RuntimeLoader status: 1 errorString: Success!` 依旧，进程持续跑了
  15+ 秒无崩溃无新增警告，`ps` 显示稳定 ~59% CPU（符合"线程化渲染循环 +
  动画持续更新"的预期负载，不是卡死/空转）。**没有做视觉截屏确认**
  （这台设备是 KMS 直出，没有 weston，没找到可用的截屏工具如
  ffmpeg/kmsgrab/fbgrab）——`onBoundsChanged` 日志打印的
  `QQuick3DBounds3(, )` 看起来是空的，但确认了 `QQuick3DBounds3`
  这个 `Q_GADGET` 没有自己的 `QDebug operator<<` 实现，用的是 Qt
  通用的属性反射打印机制，这个打印本身是否能正确显示 `QVector3D`
  属性值存疑（可能是打印格式的问题，不是 bounds 真的是零）。

  **2026-11-03 后续排查确认：确实是打印格式问题，不是真的零。** 把
  `runtime_loader.qml` 里的 `console.log("...", bounds)` 改成分别打印
  `bounds.minimum`/`bounds.maximum`（这两个是 `QVector3D`，有正常的
  调试打印实现）后，真机复测拿到了合理的数值：
  ```
  min= QVector3D(-0.569137, -1.56164e-08, -0.131) max= QVector3D(0.569137, 1.50655, 0.180954)
  ```
  这是一个站立在原点附近、身高约 1.5 米的人形模型——**几何体是对的，
  导入是对的**。`QQuick3DBounds3` 这个 `Q_GADGET` 没有自己的
  `QDebug`/`console.log` 友好的字符串转换，直接 `console.log(bounds)`
  在 QML 里就是打印不出内容，纯粹是打印方式的问题，和渲染正确性无关。

- [x] **视觉确认：已完成，模型可见，动画在动。** 真机接显示器肉眼确认。
  中间发现并修复了第三个问题——不是 Qt 的 bug，是这份测试用 QML
  场景（`runtime_loader.qml`）自己的相机参数从来没被实际跑通过验证：
  `PerspectiveCamera` 的默认 `clipNear` 是 **10**（Quick3D 的历史默认值
  假设场景单位是"百"量级），但这个场景相机放在 `position: (0, 1.2,
  3.5)`，离原点只有 3.5 个单位，而 CesiumMan.glb 是按**米**为单位的
  人形模型（身高约 1.5），整个模型都比近裁剪面还近，**全部被裁掉了**——
  肉眼看到的是纯黑背景 + fps 文字，没有任何报错，因为这不是渲染失败，
  是"渲染了，但裁剪平面把模型整个裁没了"。这段 QML 是在整条链路都跑不
  通的时候写的，相机参数从来没有机会被实际验证过，直到今天 ES3 bug 和
  关键帧 bug 都修好、第一次真的渲染到了这一步才暴露出来。

  修复：给 `PerspectiveCamera` 显式设置 `clipNear: 0.1` / `clipFar:
  100`（匹配米级场景单位），重新编译、真机复测，**肉眼确认模型可见、
  骨骼动画在正常播放**。改动见 `runtime_loader.qml`。

- [ ] 帧率/内存验证——三个问题都修复后还没正式测，下一步可以做（复用
  `scripts/sample_rss.sh` 和已有的 fps 显示 Text 元素）。
- [ ] 清理调试用的 `[dbgpatch]` qDebug 日志（在
  `qtbase-everywhere-src-6.8.5/src/gui/opengl/platform/egl/
  qeglplatformcontext.cpp`）
- [ ] 如果修复成功：是否启动"独立 Qt 构建仓库"的规划——而且现在范围更
  明确了：不是要维护一个"改过业务逻辑的 patch"，只是要维护一份
  **正确的 configure 参数清单**（`-opengl es2` 等），风险和维护成本比
  原来设想的"改 Qt 源码逻辑"低很多。
