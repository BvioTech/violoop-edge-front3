# app — violoop-edge-kiosk-qt

Qt/QML 实现的设备触控屏 kiosk 应用，不要求视觉 1:1（无设计稿）。

## 功能

- **主页**：左上角设备状态图标（WiFi/云端/HDMI/USB/配对口）。
- **工厂测试**：19 项测试 + `/stress` 占位。
- **调试菜单**：设备状态、授权、音量、推送、升级回放、音频资源、SSH、
  恢复出厂、触摸诊断、WiFi 配置。
- **版本和升级**：轮询/推送合并、不可达宽限、失速检测、自适应轮询规则。
- **待机（Away）**：持续播放的 3D 骨骼动画（`qml/AwayPage.qml`，Qt
  Quick3D `RuntimeLoader` 运行时加载 glTF），对应 edge-front2 的
  `lib/pages/away/away_page.dart`（那边播的是预渲染 MP4，这边是实时
  渲染）。单指滑动旋转视角、双指缩放、可暂停，用来证明是实时 3D 而
  不是视频。模型见下方"结构"，页面底部可手动切换模型里的各段动画，
  从开发菜单进入。

详细进度见 `docs/progress.md`。

## 结构

```
app/
├── CMakeLists.txt
├── main.cpp
├── core/            # C++：RestClient/SseClient、StatusStore/TestVerdictStore/
│                       UpgradeStore/NavController
├── qml/              # 所有页面，flat 结构（不用子目录，避免 QML 跨目录
│                       import/qmldir 问题），TestPageFrame.qml 是 19+
│                       工厂测试子页面共用的外壳
├── assets/
│   └── away/          # AwayPage.qml 用的 glTF 模型，git-lfs 追踪（见
│                        ../.gitattributes）。VIO_DesktopPet.glb 是设计师
│                        给的吉祥物模型（19 段动画）；CesiumMan.glb 是之前
│                        的占位模型（Khronos glTF-Sample-Models，CC0），
│                        留作轻量基准。
├── scripts/
│   ├── launch-under-weston.sh   # 开发用，env var 可覆盖路径
│   ├── launch-eglfs.sh          # 开发用，不经 weston 直接上 KMS（评估中）
│   └── build-deb.sh              # 打包，见下
├── packaging/         # .deb 的 staging 内容
└── docs/
    └── progress.md
```

## 开发

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/violoop-qt6
cmake --build build

# 离屏快速验证没有 QML 加载错误（不需要真机/GPU/kiosk），建议在上真机
# kiosk 之前先跑一遍
QT_QPA_PLATFORM=offscreen timeout 5 ./build/violoop_edge_kiosk_qt

# 真机：临时顶替 violoop-edge-kiosk.service 的 ExecStart
./scripts/launch-under-weston.sh
# 或者不经 weston（见下方"不经 weston（eglfs）"）
./scripts/launch-eglfs.sh
```

链接 `/opt/violoop-qt6`（`violoop-qt6` 包），不是系统 `qt6-*` 包——后者
在这台设备上有 GLES/ESSL100 版本协商问题。

## 截图

给应用进程发 `SIGUSR1`，它会把自己的窗口存成 PNG（`core/ScreenshotOnSignal`），
已经转正，不用再旋转：

```bash
sudo kill -USR1 "$(systemctl show -p MainPID --value violoop-edge-kiosk-qt)"
# -> /var/lib/violoop-edge-kiosk-qt/screenshot-<日期>-<时间>.png
#    路径同时记在日志里：journalctl -u violoop-edge-kiosk-qt | grep ScreenshotOnSignal
```

不要用 `systemctl kill -s USR1`：它会把信号发给服务里的所有进程，包括
weston，而 weston 没有处理这个信号的理由，很可能直接退出（没有实测）。截图由应用自己完成，不依赖 `weston-screenshooter`，所以在
eglfs 下也能用。

## 不经 weston（eglfs）

**状态：评估中，默认仍然走 weston。** 开关和脚本已经写好，但真实应用还没有
在 eglfs 下上过真机。

weston 在这里只服务一个全屏窗口：旋转是 `qml/AppShell.qml` 自己做的，息屏
靠直接操作背光，面板只有一个固定模式（见"性能标杆"）。去掉它可以少一层
合成、少一个约 58 MB 的常驻进程和一个包依赖。

打开方式（装好的包）：在 `/etc/violoop/kiosk-qt` 里设
`EDGE_KIOSK_QPA=eglfs`，然后 `sudo systemctl restart violoop-edge-kiosk-qt`。
去掉这一行就回到 weston。本地构建用 `scripts/launch-eglfs.sh`。

切换默认值之前要在真机上确认的事，按顺序：

1. 触摸：四角点击、滑动方向、待机页的单指旋转和双指缩放。旋转后的触摸
   对齐只在 weston 下验证过；eglfs 下是 Qt 自己读输入设备。
2. 待机页帧率和 GPU 负载，与"性能标杆"里 weston 下的数字对比
   （`idle.default` 55.0 fps / 79%）。
3. 应用崩溃到服务重启之间屏幕上显示什么。
4. 6 小时烤机，与现有基准同条件。

已知的坑：`eglfs_kms` 自己选设备会选到 `/dev/dri/card1`（Mali 渲染节点）
然后报 "no screens available"，必须指定 `card0`，启动脚本已经处理
（来历见 `../qt3d-spike/docs/findings.md` 的"EGLFS QPA"一节）。

## 日志

规范见 os 仓库的 `docs/app-logging-self-audit.md`。本服务属于应用单元，
只有 warning 及以上会上云。

- `core/JournalLog.cpp` 装了一个 Qt 消息处理器，给每行加 `<N>` 级别前缀，
  journald 才能记对 priority。`qDebug`/`qInfo`/`qWarning`/`qCritical` 和 QML
  的 `console.log`/`info`/`warn`/`error` 依次对应 debug/info/warning/error。
  stderr 是终端时不加前缀。
- 失败用 `qWarning`/`console.error`，不要用 `console.log`（那是 debug）。
- 任何级别都只记元数据（页面名、id、计数、状态码），不记用户内容和凭据。
- 周期性失败只在状态翻转时打一条，参考 `StatusStore::onFailed`。
- 上板验证：`journalctl -p warning -u violoop-edge-kiosk-qt`。

## 性能标杆

待机页（`qml/AwayPage.qml`）在真机上的基准数据，以后改模型、灯光或 Qt 版本时
拿来对比。测于 2026-11-13 晚至 11-14 凌晨，提交 `4d08f95`。

**测试条件**

- 设备：dev1，Violoop RK3576 VLA11（8 核，8 GB 内存，Mali GPU），
  面板 1080×1240@60Hz，Qt `6.8.5-violoop3`，weston kiosk-shell
  （`repaint-window` 为默认值，见下）。
- 面板刷新率固定 60Hz，没有别的档位（2026-11-14 在 dev1 上只读确认）：
  `/sys/class/drm/card0-DSI-1/modes` 和 weston 启动日志都只有
  `1080x1240@60.0` 一个模式；面板是 `simple-panel-dsi`，设备树里只有
  `timing0` 一组时序（像素时钟 84,296,640 Hz，总行列 1108×1268，正好
  60.000 Hz）；EDID 为空，模式全部来自设备树。是否支持 VRR 没有查
  （设备上没有 `modetest`）。
- 模型：`assets/away/VIO_DesktopPet.glb` 减面版（7.5 万面，19 段动画），
  全程播放 `idle.default`。
- 渲染设置：MSAA High、六盏灯、一张 light probe
  （`assets/away/studio-probe.hdr`，曝光 0.6）、默认视角。
- 采样：`core/PerfRecorder` 约每秒一行，共 6 小时（20130 个采样，去掉开头
  30 秒的加载阶段）。

**6 小时烤机结果**

| 指标 | 平均 | 范围 | 趋势 |
| --- | --- | --- | --- |
| 帧率 | 56.1 fps | 43.6–60.4 | 无衰减，每小时均值 56.0–56.2 |
| CPU 负载（全部核） | 17.7% | 10.7–32.2% | 无趋势 |
| GPU 负载 | 82.5% | 多数 70–89% | 无趋势 |
| SoC 温度 | 47.8°C | 46.2–51.8°C | 无爬升，每小时均值 47.3–48.2°C |
| 进程内存（RSS） | 约 294 MB | 292–299 MB | 持平 |

- 全程无崩溃、无服务重启，应用没有 warning 及以上的日志，内核日志里没有
  OOM、温控降频或 GPU 报错。
- 帧率不是稳在 60：低于 55 fps 的采样占 30.6%，低于 50 fps 的占 1.5%，
  低于 45 fps 的只有 1 个。GPU 负载在 80% 以上，余量不大。

**上一轮基准（30 fps 上限下，第一版模型）**

同一天早些时候、提交 `2031e80` 的 6 小时烤机，模型是第一版（38.2 万面），
启动脚本还带着 `repaint-window=-1`：帧率 29.6 fps（24.5–30.4），CPU 12.9%，
GPU 55%，SoC 45.5°C，内存约 327 MB，同样无崩溃、无趋势。那一轮最后 47 分钟
有人在屏幕上切过动作，内存中位数高了约 5 MB 后不再上涨；反复切换动作是否
累积，两轮都没有测。

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

去掉之后两版模型在真屏上的帧率（MSAA High + probe，每项约 30 秒）：

| 模型 | 动作 | 帧率（平均 / 最低） | GPU 负载 |
| --- | --- | --- | --- |
| 第一版（38.2 万面） | `idle.default` | 43.0 / 40.1 | 77% |
| 第一版 | `working` | 48.3 / 41.3 | 85% |
| 减面版（7.5 万面，现用） | `idle.default` | 55.0 / 48.2 | 79% |
| 减面版 | `working` | 57.4 / 51.8 | 74% |

在 30 fps 上限之下，抗锯齿方式（MSAA、SSAA 到 2x）和 light probe 都只改变
GPU 负载（35%–74%），不改变帧率；去掉上限之后它们对帧率的影响还没有重测。

**复现**

待机页打开期间自动记录到
`/var/lib/violoop-edge-kiosk-qt/away-perf-<日期>-<时间>.csv`（列的含义见
`core/PerfRecorder.h`），页面右上角同时显示实时读数。无人值守烤机时让
kiosk 启动后直接进待机页：

```bash
sudo mkdir -p /etc/systemd/system/violoop-edge-kiosk-qt.service.d
printf '[Service]\nEnvironment=EDGE_KIOSK_OPEN_PAGE=AwayPage.qml\n' \
  | sudo tee /etc/systemd/system/violoop-edge-kiosk-qt.service.d/burn-in.conf
sudo systemctl daemon-reload && sudo systemctl restart violoop-edge-kiosk-qt
# 结束后删掉 burn-in.conf，再 daemon-reload 并重启服务
```

服务中途重启会另起一个新 CSV，所以文件数大于 1 就说明烤机期间重启过。

## 打包

```bash
# 需要先装 violoop-qt6（apt-get install violoop-qt6，源见
# .github/workflows/build-release.yml 或直接从 deb.violoop.ai 装）
bash scripts/build-deb.sh arm64
# -> output/violoop-edge-kiosk-qt_<version>_arm64.deb
```

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
