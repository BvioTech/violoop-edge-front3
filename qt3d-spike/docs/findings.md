# Qt Quick 3D spike — findings log

> 目的：按 `../../edge-front2/docs/display-stack-overview.md` §8 的最小验证清单，
> 评估 Qt（Wayland/EGLFS QPA + Quick3D）在 rk3576/Mali-G52 这台设备上跑 3D
> 动画（glTF + 骨骼动画）的可行性。记录时间与设备以每条为准，不是最终结论。

## 环境

- 设备：dev1（`~/.ssh/config` 别名；实测时 IP 为 192.168.1.50，接入
  `Bvio-Tech-5G`，与 `docs/display-stack-overview.md` 记录的
  192.168.31.15 不是同一次网络环境——换设备/环境需要重新核对 IP）。
- 系统：Violoop OS（Debian 12 bookworm aarch64），与 edge-front2 文档描述一致。
- Qt 版本：6.4.2（Debian bookworm 自带，`apt` 直接可装，见
  `../scripts/install-deps.sh`）。
- 测试期间**临时**把 `violoop-edge-kiosk.service`（生产 Flutter kiosk）的
  `ExecStart` 换成本 spike 的程序，通过 `systemctl edit` 的 drop-in override；
  测完已删除 override、`daemon-reload`、重启服务，确认恢复为跑
  `/opt/violoop-edge-kiosk/violoop_edge_kiosk`。过程中 kiosk 服务短暂不可用
  （累计约 10 分钟内的多次切换）。

## 结论摘要

| 验证项 | 结果 |
| --- | --- |
| Qt6 + Quick3D 能否从官方源直接装 | ✅ 能，6.4.2，见"依赖坑"一节 |
| Wayland QPA（走现有 weston kiosk-shell）能否拿到硬件加速、全屏、无黑屏 | ✅ 能 |
| EGLFS QPA（不经 weston，Qt 直接拿 KMS）能否跑起来 | ✅ 能，但要手动指定正确的 DRM 设备（见下） |
| 非蒙皮的 Quick3D 3D 动画（旋转 Cube）帧率/内存 | ✅ 稳定 59–60fps，20s 内几乎不涨；追加测了约 5 分钟，净变化为 0 |
| glTF 蒙皮动画（Khronos CesiumMan，骨骼数 38）渲染 | ❌ 两条 QPA 路径都失败：默认材质的蒙皮 shader 编译失败 |
| 旋转、触摸坐标 | 未测（这轮只有固定视角场景，见"未覆盖"） |
| 换 Qt 6.8.5（源码自己编译）是否修了 ESSL100 协商问题 | ❌ 没修，EGLFS 下复现一模一样的 `textureSize is not supported in ESSL 100`（见下方 2026-11-02 第二轮） |
| 用 `-opengl es2` 重新编译 qtbase 是否修复 ESSL100/版本协商根因 | ✅ 修复，蒙皮 shader 编译通过，`QSurfaceFormat` 正确报告 3.2（见下方 2026-11-03 第三轮） |
| 骨骼动画属性绑定报错（`Cannot set property "position"` 刷屏） | ✅ 也修复了，是 `qtquicktimeline` 里一个独立的 off-by-one bug，和 GLES 配置无关（见下方 2026-11-03 第三轮） |
| glTF 骨骼动画（蒙皮）渲染，肉眼视觉确认 | ✅ 模型可见，动画在动，真机接显示器确认（见下方 2026-11-03 第三轮）；中途还发现并修复了测试场景自己的相机裁剪平面参数错误 |

**判断**：Qt Quick3D 在这台设备上跑“3D 动画”本身是可行的——硬件加速、
weston kiosk-shell 兼容性、帧率、内存表现都通过了，且看起来明显不会复现
Flutter/GTK embedder 那条每帧 GL 对象泄漏的问题（§5 Flutter 基线约
20MB/min，这边 20 秒内几乎零增长，但只测了 20 秒，不是长时间验证，不能
直接下"没有慢速泄漏"的结论）。**但“glTF + 骨骼动画”这个具体组合目前跑不通**，
是 Qt 这两个 QPA 后端在协商 GLES 版本上的一个具体限制，**6.4.2 和从源码
编译的 6.8.5 都复现同样的问题，不是版本号的问题**，细节见下。如果产品需要
的 3D 动画不依赖骨骼蒙皮（例如模型旋转、变换动画、粒子、程序化动画），
当前证据支持可行；如果必须要蒙皮角色动画，Qt Quick3D 默认材质这条路径
目前在这台设备上走不通，需要走"未覆盖"一节里列的绕过方向（少骨骼数/
自定义 ES3 shader/直接改 Qt 源码），或换掉 Quick3D 这条技术路线。

## 2026-11-02

### 依赖坑

- Debian 的 Qt6 包拆分比预期细：`qt6-quick3d-dev` 只带 Quick3D 专属的
  `qml6-module-quick3d*`，**不会**自动装 `QtQuick`/`QtQml`/
  `QtQuick.Window` 这些基础 QML 模块包，缺了会在 QML 加载时报
  `module "QtQuick" is not installed` / `...WorkerScript...`。完整依赖
  列表见 `../scripts/install-deps.sh`（已踩坑更新）。
- `qt6-declarative-dev-tools` 没有打包通用的 `qml` 脚本运行器——改用正式
  编译一个 C++ 可执行文件（`main.cpp` + `qt_add_qml_module`），或者用本仓库
  新增的 `qml_runner.cpp`（直接 `QUrl::fromLocalFile` 加载任意 `.qml`，
  不用每次改资源重新编译）。
- 网络：dev1 和开发机当时都连不上 `deb.debian.org`（`apt-get update` 能
  成功，但下载包时报 "Network is unreachable" / "Unable to connect"，
  IPv4/IPv6 都试过）。绕过方法：在有出口的机器上过代理下载 `.deb`，`scp`
  到设备，`apt-get install -y ./*.deb` 本地安装。复测前先确认这次网络是否
  正常，不要默认会复现。

### Wayland QPA（复用现有 weston kiosk-shell）

把 `violoop-edge-kiosk.service` 的 `ExecStart` 换成
`scripts/launch-under-weston.sh`（原样复用 edge-front2 launcher 的
`weston --shell=kiosk-shell.so` 配置和 PAM/TTY seat 获取方式），跑编译好的
Qt 二进制：

- weston 日志确认选中了 EGL config、`DSI-1` 1080x1240@60Hz 模式正常，
  kiosk-shell 模块加载成功。
- Qt 端日志确认：`Using Wayland-EGL`、收到
  `xdg_toplevel.configure ... WindowFullScreen`，说明 kiosk-shell
  **认可并全屏显示了这个 Qt 客户端**——回答了 edge-front2 文档 §3 里
  "kiosk-shell 目前只服务一个客户端进程，没验证过别的客户端"的问题：
  至少单独运行时，Qt 客户端可以正常被 kiosk-shell 接纳、全屏、不黑屏，
  和 Flutter 之前验证过的路径结果一致。
- 跑 `primitive_test.qml`（旋转 Cube，非蒙皮）时，QML 里的 `console.log`
  周期打印确认 fps 稳定在 **57–59**，匹配面板 60Hz 刷新率，说明这条路径
  有真实的硬件加速渲染（不是黑屏空转）。
- `weston-screenshooter` 的一个坑：`-o` 参数**不是**输出文件路径（尽管看起来
  像），它会忽略 `-o` 指定的路径，把截图存到**运行时的当前工作目录**，
  文件名自带时间戳（`wayland-screenshot-<timestamp>.png`）。第一次没注意
  这点，截图目标目录权限也不对（脚本以 `violoop` 用户跑，输出路径所在目录
  是 `admin` 用户建的、没给其他用户写权限），两个问题都在后续确认旋转/
  居中效果时顺带修好了，拿到了真实截图（见下方"真机截图确认"）。

### EGLFS QPA（不经 weston，Qt 直接拿 KMS）

这条路径更能单独验证"Qt 自己管 EGL context、直接 swap 到 DRM"的假设
（对应 edge-front2 文档 §5 末尾"Qt 的 Wayland/EGLFS QPA 插件架构上不经过
GTK/cairo"的推测）。

- **第一次尝试直接崩溃**（`ABRT` / "Cannot create window: no screens
  available"）：`eglfs_kms` 自动探测选中了 `/dev/dri/card1`，但这台设备是
  split-GPU 架构（见 edge-front2 文档 §2/§3）：`card0` 才是 `rockchip-drm`
  真正的 KMS/scanout 设备，`card1` 不支持 `drmModeGetResources`
  （"Operation not supported"）。**修复：** 写一个 `QT_QPA_EGLFS_KMS_CONFIG`
  JSON 文件显式指定 `{"device": "/dev/dri/card0"}`，已经写进
  `scripts/launch-eglfs.sh`。这是给任何以后走 EGLFS 的框架的直接可复用结论，
  不是 Qt 专属坑——任何直接碰 KMS 的框架在这台设备上都会撞到这条。
- 指定正确设备后，`eglfs_kms` 成功拿到 `DSI1` 输出（1080×1240），创建
  GBM surface，EGL context 创建成功。
- `primitive_test.qml` 在 EGLFS 下稳定 **59–60fps**，用
  `scripts/sample_rss.sh` 连续采样 20 秒 `VmRSS`：`125328 → 125328 →
  125376 → 125376`（单位 KB），20 秒内只涨了 48KB，基本持平。**没有看到
  Flutter/GTK embedder 那种每帧新建/删除 GL 对象导致的持续增长**
  （对比基线：edge-front2 文档 §5/issue22 记录的 ~20MB/min）。这是这轮
  验证里对"Qt 路径不继承 GTK embedder 泄漏机制"这个推测最直接的正面证据，
  但只测了 20 秒，**不能据此断言长时间运行没有更慢的泄漏**，需要更长时间
  （数十分钟到数小时）复测才能下更有把握的结论。

### glTF 骨骼动画限制（两条 QPA 路径都复现）

用 Khronos 官方 `CesiumMan.glb`（骨骼数 38，标准蒙皮动画测试模型）过
`QtQuick3D.AssetUtils.RuntimeLoader` 运行时加载：

- **离屏（`QT_QPA_PLATFORM=offscreen`）导入本身是成功的**
  （`RuntimeLoader.status` 回 `Success`），说明 mesh/skin/joint 的解析链路
  没问题，不需要 `balsam` 离线转换或 assimp 插件。
- **但在真实 GPU 路径（Wayland 和 EGLFS 都一样）渲染会失败**，报：

  ```
  Failed to compile vertex shader:
  "mesh default material pipeline-- ...boneCount=38;...joint&weight=true...usesFloatJointIndices=true..."
  "textureSize is not supported in ESSL 100."
  ```

  即：Quick3D 给这个蒙皮网格生成的默认材质顶点 shader 用了
  `textureSize()`（蒙皮矩阵走纹理查找，这是常见的实现方式——骨骼数一多，
  走 uniform 数组会超限，必须用纹理存骨骼矩阵，纹理查找需要
  `texelFetch`/`textureSize`，这俩是 GLSL ES 3.00+ 才有，GLSL ES 1.00
  （对应 ESSL 100 / GLES2）没有）。
- **关键诊断**：用 `qt.rhi.general` 日志确认，`glGetString(GL_VERSION)`
  真实返回 `OpenGL ES 3.2`（Mali-G52，libmali r24p0 blob——硬件和驱动确实
  支持 ES3.2，和 edge-front2 文档 §2 的记录一致）。但 Qt 自己创建的
  context 对应的 `QSurfaceFormat` 一直报 `version 2.0`，**不管
  `QSurfaceFormat::setDefaultFormat()` 请求 `OpenGLES` + `(3,2)` 放在
  `QGuiApplication` 构造之前还是之后都没有任何区别**（两种写法、两个 QPA
  后端，四种组合结果完全一样：`Created OpenGL context
  QSurfaceFormat(version 2.0...)`，紧跟着 `OpenGL VERSION: OpenGL ES
  3.2`）。也就是说：**硬件和驱动明明是 ES3.2，但 Qt 6.4.2 的
  wayland-egl 和 eglfs_kms 这两个 QPA 后端在"告诉 Quick3D 的 shader 生成器
  这个 context 是哪个版本"这件事上，似乎都没把请求的高版本传下去**——目前
  看像是 Qt 6.4.2 这两个 EGL 系 QPA 插件的限制，不是设备驱动的限制。
  （已检索 Qt 官方文档 `qtquick3d-requirements.html`：官方只说
  "OpenGL ES 2.0 或以上，强烈建议 3.0 以上"、"自定义材质用到 ESSL 100
  不支持的 GLSL 特性是 ES2.0 上的已知限制"，没有专门提到蒙皮/骨骼动画
  这个具体限制，这段是本次实测独立发现的，不是文档里已知的坑。）
- **影响范围确认**：换成不带骨骼的简单几何体（`primitive_test.qml`
  的旋转 Cube）在同样的 context 上渲染完全正常、稳定 59-60fps——说明
  Quick3D 本身、这块 GPU、这条 EGL 路径都没问题，问题精确限定在
  "蒙皮网格的默认材质 shader 生成"这一个具体子功能上。
- **失败不会导致崩溃或泄漏**：shader 编译失败只触发一次（不是每帧重试），
  之后那个 mesh 节点就是不渲染，应用本身继续正常跑（证据：CesiumMan 场景
  在 EGLFS 下跑 15 秒，`VmRSS` 稳定在 153816KB 没有变化，之后是我手动
  kill 掉的，不是它自己崩的）。

### 真机截图确认：应用侧旋转 + 居中

`primitive_test.qml` 加了两处修改，对应 edge-front2 §4 的旋转约定：

1. 外层包一个 `Item`，`width`/`height` 与窗口的宽高对调（物理面板
   1080×1240 竖屏不变，内容逻辑尺寸变成 1240×1080），`rotation: -90`
   （逆时针）——和 `RotatedBox(quarterTurns: 3)`（顺时针转 270° ==
   逆时针 90°）是同一个变换方向。
2. `PerspectiveCamera` 去掉了之前 `position: (0, 200, 400)` 里多余的
   y 偏移，改成 `(0, 0, 400)`，直接正对原点——此前的偏移会让立方体
   (在原点) 被推到画面边缘而不是居中。

走 weston/wayland 路径截图（`weston-screenshooter`，见上面那条坑），
确认：左下角 `fps: 59.0` 文字已经竖排（从下往上读），说明整屏确实转了
90°；橙色方块的包围盒居中于画面，和预期一致。截图本身没有入库（运行时
产物），复现方法见 `scripts/launch-under-weston.sh` + 当前
`primitive_test.qml`。

### 非蒙皮场景的中期内存观察（追加验证）

把 §"EGLFS QPA"一节里跑的同一个 `primitive_test.qml`（旋转 Cube，非蒙皮）
通过 weston/wayland 路径改成**不自动退出、持续跑**，用 `scripts/sample_rss.sh`
同样的 `VmRSS` 采样方式，每 10 秒采一次、每约 60 秒（或单次跳变超过
500KB 才）记一条，连续观察了 **约 5 分钟**：

```
16:03:05  135300KB  (起始)
16:03:59  135300KB  (+0)
16:05:03  135300KB  (+0)
16:06:08  135300KB  (+0)
16:07:12  135560KB  (+260KB)
16:08:17  135300KB  (-260KB，回落)
```

5 分钟内净变化为 0，中间唯一一次 ±260KB 的小波动随后又回落，量级上是正常的
页缓存/分配器噪声，不是单调增长，和 edge-front2 文档 §5/issue22 记录的
Flutter/GTK embedder 那种 ~20MB/min 持续单调增长**性质完全不同**。这比
第一轮只测 20 秒更有说服力，但 5 分钟仍然是中期观察，不是长期（小时级）
结论——"长时间运行是否有更慢的泄漏"这条待验证项（见下）从"完全没测"
降级为"短中期没有，长期未知"，没有完全解决。

### 未覆盖 / 后续如果要继续深挖的方向

1. ~~换更新的 Qt 版本（6.5/6.6/6.7+）复测同一个 `textureSize`/ESSL100
   问题是否已经修了~~——**已在 2026-11-02 第二轮测过 6.8.5（源码编译），
   没修，结论见下方新增章节**。不用再怀疑"是不是版本号不够新"，这条
   路径排除了。
2. 骨骼数更少（比如 Qt 官方 Quick3D 示例常用的几个关节的简单骨架）是否能
   避开纹理查找路径、落回 uniform 数组实现从而在 ESSL100 上也能跑——没测，
   纯推测，需要用骨骼数可控的测试资源验证。
3. 自定义材质（手写 shader）主动声明需要 GLSL ES 300 是否能绕开默认材质
   这条自动生成路径的限制——没测。
4. 旋转：`primitive_test.qml` 已经加了应用侧 90° 逆时针旋转（外层 `Item`
   `rotation: -90`，对应 edge-front2 `RotatedBox(quarterTurns: 3)` 的同一
   约定），真机截图确认内容和居中都正确旋转了（见下一条截图）。但**触摸
   坐标完全没测**——这轮场景没有任何交互元素，edge-front2 文档 §4 提到的
   "旋转后触摸坐标系统是否对齐"这个问题仍然没有验证过。
5. 长时间（分钟到小时级）内存曲线——在上一轮 20 秒的基础上追加测了约
   5 分钟（见上面"非蒙皮场景的中期内存观察"），5 分钟内净变化为 0，比
   20 秒更有说服力，但仍然是中期观察，小时级会不会有更慢的泄漏还是没有
   结论。
6. 拿到了真实截图（`weston-screenshooter` 保存到自定义工作目录而不是
   `-o` 参数指定的路径——这是它的实际行为，不是 `-o path` 意义上的输出
   文件，这个坑也顺手记下了），确认了旋转和居中效果的像素级证据，不再
   只是帧率/日志推断。
7. 字体（CJK）、`violoop-upgrade` 集成点——edge-front2 文档 §8.5 提到的
   系统集成面，这轮完全没碰。

## 2026-11-02（第二轮）：Qt 6.8.5 源码编译复测，问题没修

确认产品需要蒙皮角色动画后，为了排除"是不是 Qt 6.4.2 版本太老"这个
假设，在 dev1 上从源码编译了 Qt 6.8.5（当时的最新 LTS 补丁版本），装到
独立 prefix `/home/admin/qt6.8`（不碰系统包），重新编译 `qt3d_spike`
链到这个新 prefix，同样用 `CesiumMan.glb` 在真机 EGLFS 路径上测。

### 编译过程

- 源码：Qt 官方 `*-everywhere-src-6.8.5.tar.xz`，模块按依赖顺序编译：
  `qtbase` → `qtshadertools` → `qtdeclarative` → `qtwayland` +
  `qtquicktimeline` → `qtquick3d`，每个模块 `qt-configure-module` +
  `ninja -j8` + `ninja install`，全部编译、安装成功，没有一个模块报错。
- **下载坑**：dev1 直接下载这几个大 tarball（尤其 qtbase/qtdeclarative/
  qtquick3d，几百 MB 到 1GB+）反复在下载到一半时被重置
  （`curl: (18) transfer closed with N bytes remaining to read`，
  `xz -t` 校验失败），换 `-C -` 续传也只能多下几 MB 就又断。**绕过方法**：
  改成在有稳定出口的机器上过代理下载，`scp` 到 dev1，`xz -t` 全部校验
  通过后再解压——和这轮之前 apt 包下载失败时用的是同一个绕过模式。
- 编译总耗时数小时（`qtdeclarative` 单个模块 3232 个构建步骤最久），
  全部在后台跑、轮询完成状态，没有遇到编译错误。

### 真机复测结果：问题原样复现

通过同样的临时 systemd override 模式（见本文件开头"环境"一节的流程，
测完立即还原），把 `qt3d_spike`（链到 `/home/admin/qt6.8`，
`LD_LIBRARY_PATH`/`QML2_IMPORT_PATH` 指到这个 prefix）通过 EGLFS 路径
跑 `CesiumMan.glb`，日志：

```
qt.rhi.general: Created OpenGL context QSurfaceFormat(version 2.0, ...)
qt.rhi.general: OpenGL VENDOR: ARM RENDERER: Mali-G52 VERSION: OpenGL ES 3.2 ...
WARN: Model has non-integer type indices for skinning but current RHI backend doesn't support it!
Failed to compile vertex shader: textureSize is not supported in ESSL 100.
```

和 Qt 6.4.2 那一轮**一字不差的同一个失败模式**：硬件/驱动真实报告
ES 3.2，但 Qt 创建的 context 在 `QSurfaceFormat` 里还是 version 2.0，
蒙皮默认材质 shader 因此在 ESSL100 下编译失败。另外还多了一条
6.4.2 那轮日志里没特别注意到的 WARN（`non-integer type indices for
skinning but current RHI backend doesn't support it`）——可能指向
CesiumMan 的 joint indices 类型和这条 RHI 后端的整数索引支持不匹配，
是同一个"RHI/GLES 版本协商"限制的另一个表现，还是一个独立的次要问题，
没有继续深挖，不影响这轮的核心结论。

**结论**：**ESSL100/textureSize 这个限制不是 Qt 6.4.2 的版本 bug，
6.8.5（当时最新 LTS 补丁版）原样复现。** 排除了"换新版本就能解决"这个
最低成本的假设。往下如果还要让 Quick3D 默认材质路径跑通蒙皮，只能走
"未覆盖"一节里排第 2、3 条的方向（少骨骼数绕开纹理查找 / 自定义材质
手写 ES3 shader），或者直接改 Qt 源码本身（已经有源码编译环境，改动
成本可控，但需要先确认问题具体出在哪一层——QPA 插件协商、RHI 后端，
还是 Quick3D 的 shader 生成器硬编码假设——这部分还没做）。

这轮编译验证的副作用：确认了 dev1 上从源码编译 Qt（含 qtbase/
qtdeclarative/qtwayland/qtquicktimeline/qtquick3d 全套）本身是可行的，
除了下载环节的网络问题之外没有遇到编译障碍——如果后面决定要自己维护一个
定制 Qt（无论是为了修这个 bug 还是其他原因），这轮摸出来的模块编译顺序和
`qt-configure-module` 用法可以直接复用。

排除了"换新版本"这条最低成本方向之后，剩下两条路分头推进，各自独立成文：

- `plan-custom-material-shader.md`——自定义材质手写 ES3 shader，绕开
  Quick3D 默认材质生成路径。**后续读源码确认这条路被同一个根因堵死，
  已经不再投入**（CustomMaterial 的 shader 最终走的是和默认材质相同的
  `QSSGShaderCache`/`QShaderBaker` 运行时烘焙流程，同样依据那个卡在
  2.0 的 `QSurfaceFormat` 决定只烤 ESSL100，手写 shader 绕不开）。
- `plan-patch-qt-source.md`——直接改 Qt 源码，定位并修复版本协商 bug
  本身（已有 dev1 上的源码编译环境可以直接复用）。**已经定位到具体是
  `qssgrendershadercache.cpp` 读 `h->context->format().majorVersion()`
  这一步在转发错误值，且确认 `qtbase` 里理论上该负责回填真实版本的
  `QEGLPlatformContext::updateFormatFromGL()` 本身解析逻辑没问题——剩下
  唯一要查的是这个函数有没有在真机这个 context 上真正被调用**，细节见
  该文档。

## 2026-11-03（第三轮）：根因找到并修复——不是 Qt 逻辑 bug，是构建配置错了

在 `updateFormatFromGL()` 里加 `qDebug()` 实测后发现：这个函数**确实被
调用了**，`eglMakeCurrent()` 也**确实成功**，但紧接着的
`glGetString(GL_VERSION)` 却返回 **null**。一路查到链接层：
`libQt6Gui.so.6` 链接的是 **GLVND 桌面 OpenGL 分发层**
（`libOpenGL.so.0`/`libGLX.so.0`），不是 Mali 的 `libGLESv2.so`——
因为当初 `qt-configure-module` 在 dev1 这台装了 Mesa 开发包的机器上
自动探测成了 `FEATURE_opengl_desktop=ON`、`FEATURE_opengles2=OFF`。
GLVND 自己维护一套"当前 context"记账，根本不知道 Mali 的 `libEGL`
刚刚设的那个 context，所以查版本号时直接返回空。**这不是 Qt 的版本
协商逻辑有 bug，是这套 Qt 构建从一开始就用了错的 OpenGL 后端。**

用 `-opengl es2` 重新 `configure` + 完整重新编译 qtbase（以及连带
重新编译/链接 qtshadertools/qtdeclarative/qtwayland/qtquicktimeline/
qtquick3d 这五个依赖它的模块）之后，真机复测：

```
qt.rhi.general: Created OpenGL context QSurfaceFormat(version 3.2, ...)
qml: RuntimeLoader status: 1 errorString: Success!
```

**不再出现** `Failed to compile vertex shader: textureSize is not
supported in ESSL 100`，也不再出现 `WARN: Model has non-integer type
indices for skinning`。**ESSL100/GLES 版本协商这个根因 bug 确认修复**，
细节（含完整调试过程、具体 configure 参数）见
`plan-patch-qt-source.md`。

但同一次测试也暴露了一个新的、独立的问题：蒙皮 shader 真的开始跑之后，
日志被刷爆（几秒内一万多条）：

```
Could not find any constructor for value type QQuickVector3DValueType to call with value QVariant(Invalid)
Cannot set property "position"
```

定位到 `QtQuickTimeline` 的 `QQuickKeyframeGroup::setProperty()`——
`RuntimeLoader` 运行时是靠动态生成 `QtQuickTimeline` 的 Keyframe 对象
驱动 glTF 骨骼动画的，某个动画通道算出来的值是无效的，写不进
`position` 属性。这是 ES3 bug 修好之后才有机会暴露出来的（之前 shader
编译不过，这段动画代码根本跑不到）。

**这个也查清楚并修复了**：根因是 `qtquicktimeline` 的
`QQuickKeyframeGroupPrivate::loadKeyframes()` 里一个 off-by-one 的
循环边界——`for (i = 0; i < arraySize - frameSize; i += frameSize)`
永远漏掉每个通道的最后一个关键帧，对只有 1 个关键帧的通道
（`arraySize == frameSize`）直接变成一次都不执行，导致这个属性的
关键帧列表完全是空的，每帧都尝试写一个无效值。这是**真实存在于 Qt
6.8.5 本身的独立 bug**，和这轮的 GLES 构建配置无关，只是之前蒙皮
shader 编译不过，这段代码没机会被注意到。改成
`for (i = 0; i + frameSize <= arraySize; i += frameSize)` 后重新
编译 `qtquicktimeline`，真机复测：这组警告完全消失（0 条），进程
稳定跑了 15+ 秒、CPU 占用 ~59%（符合持续渲染+动画更新的预期负载）。
**没有做视觉截屏确认**（设备没有现成的截屏手段），细节、代码位置和
验证过程见 `plan-patch-qt-source.md`。

**真机接显示器肉眼确认后，又发现并修复了第三个问题**——这次不是 Qt 的
bug，是测试场景自己的相机参数从来没有被真正验证过。真机跑起来后，屏幕
上只有黑色背景和 fps 文字，看不到任何模型。查下来发现 `RuntimeLoader`
的 `bounds` 其实是正常的（`console.log` 直接打印整个 `bounds` 对象
看起来是空的，纯粹是 `QQuick3DBounds3` 这个 `Q_GADGET` 没有自己的字符
串转换实现，换成分别打印 `bounds.minimum`/`bounds.maximum` 后看到的是
合理数值：`min=(-0.57, ~0, -0.13) max=(0.57, 1.51, 0.18)`，一个站在
原点附近、身高约 1.5 米的人形——几何体、导入都是对的）。真正的问题是
`runtime_loader.qml` 里 `PerspectiveCamera` 没有显式设置
`clipNear`，用的是 Quick3D 的默认值 **10**（这个默认值是给"场景单位
在百量级"的旧习惯假设的），但这个场景相机只放在离原点 3.5 个单位的地方，
而 CesiumMan.glb 是按**米**为单位的模型（身高 1.5），**整个模型比近
裁剪面还近，全部被裁掉了**——不是渲染失败，是渲染了但裁剪没了，所以
没有任何报错，只有黑屏。这段 QML 是在整条链路都跑不通的时候写的，从
来没机会被实际验证过相机参数，直到前两个 bug 都修好、第一次真的渲染到
这一步才暴露出来。

修复：给 `PerspectiveCamera` 显式加
`clipNear: 0.1` / `clipFar: 100`（匹配米级场景单位），改动在
`runtime_loader.qml`。重新编译、真机复测，**肉眼确认：模型可见，骨骼
动画在正常播放**。

**结论：glTF 骨骼动画在这台设备上确认可行。** 这一轮一共定位并修复了
三个独立问题：
1. qtbase 构建配置错误（桌面 OpenGL 而不是 GLES）——根因 bug，
   导致蒙皮 shader 编译失败。
2. `qtquicktimeline` 的关键帧加载 off-by-one 循环边界——Qt 自身的
   独立 bug，蒙皮 shader 能跑之后才暴露。
3. `runtime_loader.qml` 相机裁剪平面参数错误——测试脚本自己的问题，
   前两个 bug 修好之后才有机会被看见。
三个问题都解决后，glTF 骨骼动画（蒙皮）在 dev1 的真实硬件
（rk3576/Mali-G52）上渲染正确，肉眼确认骨骼动画正常播放。还没做的：
正式的帧率/内存验证（现在有了能看见画面的环境，下一步可以用已有的
`scripts/sample_rss.sh` 和 fps Text 元素做）。
