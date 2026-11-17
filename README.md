# edge-front3 — violoop-edge-kiosk-qt

许可证：[GPL-3.0-or-later](LICENSE)，吉祥物模型为专有素材，见文末“许可说明”。

设备触控屏 kiosk 前端，Qt/QML 实现，不要求视觉 1:1（无设计稿）。

## 功能

- **主页**：实时渲染的 3D 吉祥物（`qml/HomePage.qml`，Qt Quick3D
  `RuntimeLoader` 运行时加载 glTF），对应 edge-front2 的待机页
  `lib/pages/away/away_page.dart`（那边播的是预渲染 MP4）。视角固定，
  唯一的手势是点击。播哪段动画由
  `src/MascotDirector` 决定：没事时待机和随机小动作交替，点模型的左、中、
  右会有反应。后端状态经
  `src/MascotInputs` 接进来，有离线、等待审批、工作、完成、异常和实时对话，见
  `docs/user-model-spec.md` 的"动画"一节。左上角是设备状态
  图标（WiFi/云端/HDMI/USB/配对口），右上角"开发"按钮进开发菜单。
- **工厂测试**：19 项测试 + `/stress` 占位。
- **开发菜单**：工厂测试、调试、版本和升级的入口。
- **调试**：设备状态、授权、音量、推送、升级回放、音频资源、SSH、
  恢复出厂、触摸诊断、WiFi 配置。
- **版本和升级**：轮询/推送合并、不可达宽限、失速检测、自适应轮询规则。

详细进度见 `docs/progress.md`。

## 结构

```
.
├── CMakeLists.txt
├── main.cpp
├── src/             # C++：RestClient/SseClient、StatusStore/TestVerdictStore/
│                       UpgradeStore/NavController；主页用的 ClipPlayer
│                       （播放动画片段）、MascotDirector/ActionCatalog
│                       （决定播哪一段）、MascotInputs（把后端状态
│                       交给导演）、SystemStats/PerfRecorder（性能
│                       读数和记录）；JournalLog（日志级别）、
│                       UnixSignalWatcher/Screenshot（SIGUSR1 截图）、StateDir
├── tests/            # 单元测试（QtTest），`ctest --test-dir build`
├── qml/              # 所有页面，flat 结构（不用子目录，避免 QML 跨目录
│                       import/qmldir 问题），TestPageFrame.qml 是 19+
│                       工厂测试子页面共用的外壳；主页是 HomePage.qml
│                       （点击和入口按钮）+ MascotScene.qml（模型、灯光、相机）
│                       + PerfOverlay.qml（性能读数）
├── assets/
│   └── away/          # 主页用的 glTF 模型，git-lfs 追踪（见
│                        .gitattributes）。VIO_DesktopPet.glb 是设计师
│                        给的吉祥物模型（20 段动画），actions.json 说明
│                        各段动画的用途；CesiumMan.glb 是之前
│                        的占位模型（Khronos glTF-Sample-Models，CC-BY 4.0），
│                        文件还在仓库里，但不再打进应用（要用它得先
│                        加回 CMakeLists.txt 的 RESOURCES）。
├── scripts/
│   ├── launch-dev.sh            # 开发用：用包里的同一个启动脚本跑本地
│   │                              构建，参数 wayland（默认）或 eglfs
│   └── build-deb.sh              # 打包，见下
├── packaging/         # .deb 的 staging 内容
├── docs/
│   ├── progress.md              # 实现进度和真机验证记录
│   └── user-model-spec.md       # 用户自带模型应符合的格式
└── demos/             # 正式实现之前的验证代码，各自独立构建，不参与
    │                    应用的构建和打包
    ├── qt3d-spike/              # Qt Quick 3D 角色动画渲染验证（含骨骼/
    │                              蒙皮动画）
    └── ui-interface-spike/      # UI 模式覆盖与 REST/SSE 接口对接验证
```

## 开发

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/violoop-qt6
cmake --build build

# 单元测试
LD_LIBRARY_PATH=/opt/violoop-qt6/lib ctest --test-dir build --output-on-failure

# 离屏快速验证没有 QML 加载错误（不需要真机/GPU/kiosk），建议在上真机
# kiosk 之前先跑一遍
QT_QPA_PLATFORM=offscreen timeout 5 ./build/violoop_edge_kiosk_qt

# 真机：临时顶替 violoop-edge-kiosk.service 的 ExecStart
./scripts/launch-dev.sh
# 或者不经 weston（见下方"不经 weston（eglfs）"）
./scripts/launch-dev.sh eglfs
```

链接 `/opt/violoop-qt6`（`violoop-qt6` 包），不是系统 `qt6-*` 包——后者
在这台设备上有 GLES/ESSL100 版本协商问题。

## 截图

给应用进程发 `SIGUSR1`，它会把自己的窗口存成 PNG（`src/Screenshot`），
已经转正，不用再旋转：

```bash
sudo kill -USR1 "$(systemctl show -p MainPID --value violoop-edge-kiosk-qt)"
# -> /var/lib/violoop-edge-kiosk-qt/screenshot-<日期>-<时间>.png
#    路径同时记在日志里：journalctl -u violoop-edge-kiosk-qt | grep Screenshot:
```

截图由应用自己完成，不依赖 `weston-screenshooter`，所以在 eglfs 下也能用。
文件不会自动清理，用完请自己删。

`SIGUSR2` 是另一个命令：开始或停止主页的性能记录，见"性能标杆"的"复现"。
启动脚本在应用起来之前忽略这两个信号，所以服务刚重启时发过去只会被丢掉，
不会把服务杀掉。

## 不经 weston（eglfs）

**结论（2026-11-14，dev1 实测）：能跑，但待机页慢约 20%，默认继续走 weston。**
开关和脚本留着，供以后换驱动或换场景时重测。

weston 在这里只服务一个全屏窗口：旋转是 `qml/AppShell.qml` 自己做的，息屏
靠直接操作背光，面板只有一个固定模式（见"性能标杆"）。去掉它可以少一个
约 58 MB 的常驻进程和一个包依赖，这是当初想去掉它的理由。

打开方式（装好的包）：在 `/etc/violoop/kiosk-qt` 里设
`EDGE_KIOSK_QPA=eglfs`，然后 `sudo systemctl restart violoop-edge-kiosk-qt`。
去掉这一行就回到 weston。本地构建用 `scripts/launch-dev.sh eglfs`。

**实测**

同一个二进制（提交 `1ea67a0`）、同一场景（待机页 `idle.default`，MSAA High +
probe），只换启动脚本，各跑约 1 分钟：

| | 帧率（平均 / 最低） | GPU 负载 | 每帧周期 | 其中 swap |
| --- | --- | --- | --- | --- |
| weston | 56.1 / 48.8 fps | 80% | 17.1 ms | 13.9 ms |
| eglfs | 44.0 / 39.8 fps | 65% | 21.8 ms | 18.8 ms |
| eglfs + `QT_QPA_EGLFS_KMS_ATOMIC=1` | 约 46 fps | - | 21.3 ms | 18.5 ms |

- eglfs 下画面正确，应用自己拿到 DSI-1，libinput 认出了触摸屏
  （`focaltech_ft3519`），`SIGUSR1` 截图可用。触摸对齐和手势没有用手测。
- 慢不是因为算力不够：GPU 已在最高频（900 MHz）而负载只有 65%，CPU 也有
  余量。每帧渲染提交只要约 2 ms，时间花在 swap 上。
- 推断的原因（没有工具直接证实，设备上没有 strace/perf）：libmali 的 GBM
  平台上 `eglSwapBuffers` 要等 GPU 把这一帧画完才返回，CPU 准备下一帧和
  GPU 画这一帧不能并行，每帧约 8 ms CPU + 14 ms GPU 串行成 22 ms；Wayland
  平台不等，两者重叠，周期就是 GPU 的约 17 ms。依据：在渲染后立刻
  `glFlush` 让 GPU 提前开工，总周期不变（22.3 ms），只是时间从 swap 挪到了
  render（render 2 → 8 ms，swap 19 → 13 ms，且 swap 集中在 13 ms）。
- 所以这是驱动（`libmali-bifrost-g52-g24p0-x11-wayland-gbm` 1.9-3）的行为，
  不是 Qt 的参数能调的；没有在 libmali 里找到相关开关。
- 轻场景不受影响：`qt3d-spike` 的旋转立方体在 eglfs 下是 59–60 fps。GPU
  时间接近或超过一帧的场景才会吃这个亏。

**抗锯齿在两种后端下都生效（2026-11-16）**

怀疑过 eglfs 下 MSAA 没起作用，于是在 dev1 上对 weston/eglfs × MSAA High/
关闭四种组合各截了 5 张图（`SIGUSR1`，默认视角）放大对比，并经人眼在屏幕上
确认：

- 开 MSAA 时模型外轮廓平滑，关掉后是阶梯，weston 和 eglfs 表现一致。
- 模型内部的细线（青色发光边、面罩边缘的细高光）在四种组合下都有锯齿：
  MSAA 只处理几何轮廓，不处理材质和高光内部的走样。看着"像没开抗锯齿"的
  就是这部分。
- eglfs 下关掉 MSAA，帧率从 45.1 fps / GPU 65% 升到 58.6 fps / GPU 46%
  （`idle.default`，各约 1 分钟）。也就是 eglfs 的差距只在 GPU 时间接近一帧
  时出现，和上面的推断一致。
- 当时两种后端的界面字体不同：Qt 的默认字体族由平台插件决定，wayland 是
  "Sans Serif"（设备上解析成 Noto Sans），eglfs 是 DejaVu，后者更宽，按钮和
  行距都跟着变。现在 `main.cpp` 统一指定 "Sans Serif"，两边的排版一致
  （dev1 截图对比确认；字形的细微渲染差异还在）。3D 模型本身的渲染看不出
  区别。

截图取的是应用自己渲染出的画面，不经过面板。

逐帧耗时的取法：给服务加
`Environment="QT_LOGGING_RULES=qt.scenegraph.time.renderloop=true"`，
`scripts/launch-dev.sh` 会沿用它，日志在 `$RUNTIME_DIRECTORY/app.log`。

已知的坑：`eglfs_kms` 自己选设备会选到 `/dev/dri/card1`（Mali 渲染节点）
然后报 "no screens available"，必须指定 `card0`，启动脚本已经处理
（来历见 `demos/qt3d-spike/docs/findings.md` 的"EGLFS QPA"一节）。

## 日志

规范见 os 仓库的 `docs/app-logging-self-audit.md`。本服务属于应用单元，
只有 warning 及以上会上云。

- `src/JournalLog.cpp` 装了一个 Qt 消息处理器，给每行加 `<N>` 级别前缀，
  journald 才能记对 priority。`qDebug`/`qInfo`/`qWarning`/`qCritical` 和 QML
  的 `console.log`/`info`/`warn`/`error` 依次对应 debug/info/warning/error。
  stderr 是终端时不加前缀。
- 失败用 `qWarning`/`console.error`，不要用 `console.log`（那是 debug）。
- 任何级别都只记元数据（页面名、id、计数、状态码），不记用户内容和凭据。
- 周期性失败只在状态翻转时打一条，参考 `StatusStore::onFailed`。
- 上板验证：`journalctl -p warning -u violoop-edge-kiosk-qt`。

## 性能标杆

3D 场景（`qml/HomePage.qml`）在真机上的基准数据，以后改模型、灯光或 Qt 版本时
拿来对比。测于 2026-11-14 09:22–15:22，提交 `f589050`。那时这个场景在
单独的"待机页"里（从开发菜单进），本节和"不经 weston（eglfs）"一节里说的
待机页就是现在的主页。

**测试条件**

- 设备：dev1，Violoop RK3576 VLA11（8 核，8 GB 内存，Mali GPU），
  面板 1080×1240@60Hz，Qt `6.8.5-violoop3`，weston kiosk-shell
  （`repaint-window` 为默认值，见下）。
- 面板刷新率固定 60Hz，没有别的档位（2026-11-14 在 dev1 上只读确认）：
  `/sys/class/drm/card0-DSI-1/modes` 和 weston 启动日志都只有
  `1080x1240@60.0` 一个模式；面板是 `simple-panel-dsi`，设备树里只有
  `timing0` 一组时序（像素时钟 84,296,640 Hz，总行列 1108×1268，正好
  60.000 Hz）；EDID 为空，模式全部来自设备树。CRTC 上有
  `variable refresh rate` 等属性（eglfs 的日志里列出，值为 0，即未启用），
  没有试过打开。
- 模型：`assets/away/VIO_DesktopPet.glb` 减面版（7.5 万面，19 段动画），
  全程播放 `idle.default`。
- 渲染设置：MSAA High、六盏灯、设计师的场景——环境光
  `visor-studio.hdr`，脸部单独的 probe `display-studio.hdr`，面罩用
  `canopy.frag` 玻璃着色器（反射图 `optical-studio.hdr`），固定相机
  （FOV 35°，距离 5.125）。
- 采样：`src/PerfRecorder` 约每秒一行，共 6 小时（19199 个采样，去掉开头
  30 秒的加载阶段）。

**6 小时烤机结果**

| 指标 | 平均 | 范围 | 趋势 |
| --- | --- | --- | --- |
| 帧率 | 53.5 fps | 40.0–60.4 | 无衰减，每小时均值 53.4–53.7 |
| CPU 负载（全部核） | 17.4% | 10.8–31.3% | 无趋势 |
| GPU 负载 | 85.5% | 多数 69–94% | 无趋势，每小时均值 85.1–86.0% |
| SoC 温度 | 48.2°C | 46.2–51.8°C | 无爬升，每小时均值 48.1–48.3°C |
| 进程内存（RSS） | 约 302 MB | 299–306 MB | 持平 |

- 全程无崩溃、无服务重启，应用没有 warning 及以上的日志，内核日志里没有
  OOM、温控降频或 GPU 报错。全程没有人碰过屏幕。
- 帧率不是稳在 60：低于 55 fps 的采样占 64.9%，低于 50 fps 的占 14.7%，
  低于 45 fps 的占 0.3%，没有低于 40 fps 的。
- GPU 余量很小：负载中位数 89%，2.8% 的采样在 95% 以上。再往场景里加东西
  大概率直接掉帧。
- 进待机页到出第一帧约 14 秒，到帧率稳定约 17 秒（上一轮约 9 秒）。这段
  时间 GPU 负载 100%，内存短暂到 327 MB。多出来的几秒花在哪一步没有查。

**上一轮基准（同一模型，自制环境光）**

2026-11-13 晚至 11-14 凌晨、提交 `4d08f95` 的 6 小时烤机，模型相同，但环境
光是一张自制的 probe（曝光 0.6），面罩是模型自带的材质，相机按包围盒自动
取景：

| 指标 | 上一轮 | 这一轮 |
| --- | --- | --- |
| 帧率（平均） | 56.1 fps | 53.5 fps |
| 低于 50 fps 的采样 | 1.5% | 14.7% |
| CPU 负载 | 17.7% | 17.4% |
| GPU 负载 | 82.5% | 85.5% |
| SoC 温度 | 47.8°C | 48.2°C |
| 进程内存（RSS） | 约 294 MB | 约 302 MB |

那一轮同样无崩溃、无趋势。

**更早的基准（30 fps 上限下，第一版模型）**

2026-11-13 白天、提交 `2031e80` 的 6 小时烤机，模型是第一版（38.2 万面），
启动脚本还带着 `repaint-window=-1`：帧率 29.6 fps（24.5–30.4），CPU 12.9%，
GPU 55%，SoC 45.5°C，内存约 327 MB，同样无崩溃、无趋势。那一轮最后 47 分钟
有人在屏幕上切过动作，内存中位数高了约 5 MB 后不再上涨；反复切换动作是否
累积，三轮都没有测。

**weston 的 `repaint-window` 曾把帧率压在 30**

`repaint-window=-1` 是从 edge-front2 的启动脚本抄来的。这个值让 weston
每次合成都赶不上下一次刷新，所有客户端在这块 60Hz 屏上都只有 30 fps，和
模型、灯光、抗锯齿无关：只画一个旋转立方体（每帧渲染不到 1 ms）也是
29.6 fps。去掉这一行（weston 默认 7 ms）后恢复正常，启动脚本现在不再
设置它。

只改这一项的对照（`qt3d-spike`，CesiumMan）：

| `repaint-window` | 帧率 |
| --- | --- |
| -1 | 28–29 |
| 0 | 28 |
| 7 | 46–57 |
| 不设置（默认 7） | 48–55 |

去掉之后两版模型在真屏上的帧率（MSAA High + 当时的自制 probe，每项约 30 秒）：

| 模型 | 动作 | 帧率（平均 / 最低） | GPU 负载 |
| --- | --- | --- | --- |
| 第一版（38.2 万面） | `idle.default` | 43.0 / 40.1 | 77% |
| 第一版 | `working` | 48.3 / 41.3 | 85% |
| 减面版（7.5 万面，现用） | `idle.default` | 55.0 / 48.2 | 79% |
| 减面版 | `working` | 57.4 / 51.8 | 74% |

在 30 fps 上限之下，抗锯齿方式（MSAA、SSAA 到 2x）和 light probe 都只改变
GPU 负载（35%–74%），不改变帧率；去掉上限之后它们对帧率的影响还没有重测。

**复现**

主页右上角一直显示实时读数。写文件默认是关的，要显式打开：

```bash
# 开始记录；再发一次停止
sudo kill -USR2 "$(systemctl show -p MainPID --value violoop-edge-kiosk-qt)"
# -> /var/lib/violoop-edge-kiosk-qt/away-perf-<日期>-<时间>.csv
```

列的含义见 `src/PerfRecorder.h`。只有主页显示着的时候才有数据写入；
每次开始记录都新建一个文件，用完请自己删，没有自动清理。

无人值守烤机时让 kiosk 启动后就开始记录（主页就是 3D 场景，不用再指定
页面）：

```bash
sudo mkdir -p /etc/systemd/system/violoop-edge-kiosk-qt.service.d
printf '[Service]\nEnvironment=EDGE_KIOSK_RECORD_PERF=1\n' \
  | sudo tee /etc/systemd/system/violoop-edge-kiosk-qt.service.d/burn-in.conf
sudo systemctl daemon-reload && sudo systemctl restart violoop-edge-kiosk-qt
# 结束后删掉 burn-in.conf，再 daemon-reload 并重启服务
```

用环境变量而不是信号，是因为服务中途重启后信号打开的记录不会恢复。
重启会另起一个新 CSV，所以文件数大于 1 就说明烤机期间重启过。

## 打包

```bash
# 需要先装 violoop-qt6（apt-get install violoop-qt6，源见
# .github/workflows/build-release.yml 或直接从 deb.violoop.ai 装）
bash scripts/build-deb.sh arm64
# -> output/violoop-edge-kiosk-qt_<version>_arm64.deb
```

版本号在仓库根目录的 `version.txt` 文件里，打包时由 `scripts/build-deb.sh` 读取。
发版用 `scripts/version.sh [major|minor|patch]`（默认 patch）：它推进
`version.txt`、提交、打 `v<版本>` tag 并一起推送，工作区不干净时拒绝执行；
`DRY_RUN=1` 只打印下一个版本号。

CI（`.github/workflows/build-release.yml`，仓库根目录）：`ubuntu-22.04-arm`
原生 runner、`v*` tag 触发发布（创建 GitHub Release；apt 仓轮询收录，
流水线不再发 webhook）。

**包名/服务名和生产 Flutter 包（`violoop-edge-front`）完全独立**，不设
`Provides`/`Replaces`/`Conflicts`，安装本包不会顶掉生产 kiosk，`postinst`
也不会自动 `enable`/`start` 自己的服务。两个 kiosk 服务抢同一个
`seat0`/`tty7`，需手动切换：

```bash
sudo systemctl disable --now violoop-edge-kiosk.service
sudo systemctl enable --now violoop-edge-kiosk-qt.service
```

## 许可说明

本仓库自己的代码按 **GPL-3.0-or-later** 发布，全文见 [LICENSE](LICENSE)，
吉祥物模型除外（见下）。

原因：角色动画依赖 `Qt Quick 3D` 模块，该模块开源版只有 GPL-3.0-only
授权（`LicenseRef-Qt-Commercial OR GPL-3.0-only`，无 LGPL 选项，以
`qt/qtquick3d` 在 `v6.8.5-lts-lgpl` 的 `REUSE.toml` 和源文件头为准），
因此本仓库及最终依赖该模块的产品线按 GPL-3 开源发布。

- deb 包里的 `/usr/share/doc/violoop-edge-kiosk-qt/copyright`
  （源文件 `packaging/debian/copyright`）写明了许可证和源码地址：包版本
  `X.Y.Z` 对应本仓库的 git tag `vX.Y.Z`。
- 运行时依赖的 `violoop-qt6`（Qt 6.8.5）是另一个包，构建脚本和各模块
  的上游 ref 在 <https://github.com/BvioTech/violoop-qt-build>。
- 打进二进制的三张 HDR（`assets/away/*.hdr`）和 `canopy.frag` 是设计师
  为 BvioTech 制作的，版权归 BvioTech，和代码一样按 GPL-3.0-or-later
  发布。
- **吉祥物模型不在 GPL 之内**：`assets/away/VIO_DesktopPet.glb` 是设计师
  为 BvioTech 制作的专有素材，版权归 BvioTech，保留所有权利，仓库公开
  不等于授权他人使用；条款见 `copyright` 的
  `LicenseRef-BvioTech-Proprietary`。它是程序运行时读取的 glTF 数据，
  但目前通过 Qt 资源系统存放在可执行文件里（`CMakeLists.txt` 的
  `RESOURCES`）。
- `assets/away/CesiumMan.glb` 是 CC-BY 4.0（Cesium 捐赠，另有 Cesium
  商标条款），不打进发布包。
