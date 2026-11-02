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

**判断**：Qt Quick3D 在这台设备上跑“3D 动画”本身是可行的——硬件加速、
weston kiosk-shell 兼容性、帧率、内存表现都通过了，且看起来明显不会复现
Flutter/GTK embedder 那条每帧 GL 对象泄漏的问题（§5 Flutter 基线约
20MB/min，这边 20 秒内几乎零增长，但只测了 20 秒，不是长时间验证，不能
直接下"没有慢速泄漏"的结论）。**但“glTF + 骨骼动画”这个具体组合目前跑不通**，
是 Qt 6.4.2 这两个 QPA 后端在协商 GLES 版本上的一个具体限制，细节见下。
如果产品需要的 3D 动画不依赖骨骼蒙皮（例如模型旋转、变换动画、粒子、
程序化动画），当前证据支持可行；如果必须要蒙皮角色动画，需要先解决下面
这条限制或验证更新的 Qt 版本。

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

1. **换更新的 Qt 版本（6.5/6.6/6.7+）复测**同一个 `textureSize`/ESSL100
   问题是否已经修了——这是最值得先做的下一步，因为现象看起来像是 Qt
   这两个 QPA 后端的版本协商限制，而不是设备硬件的硬限制。Debian
   bookworm 只有 6.4.2，要测更新版本需要从源码编译或找第三方仓库/
   Qt 官方在线安装器的 arm64 包，工作量比这轮大。
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
