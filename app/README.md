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
```

链接 `/opt/violoop-qt6`（`violoop-qt6` 包），不是系统 `qt6-*` 包——后者
在这台设备上有 GLES/ESSL100 版本协商问题。

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
拿来对比。测于 2026-11-13，提交 `2031e80`。

**测试条件**

- 设备：dev1，Violoop RK3576 VLA11（8 核，8 GB 内存，Mali GPU），
  面板 1080×1240@60Hz，Qt `6.8.5-violoop3`，weston kiosk-shell。
- 模型：`assets/away/VIO_DesktopPet.glb`（38.2 万面，19 段动画）。
- 渲染设置：MSAA High、六盏灯、一张 light probe
  （`assets/away/studio-probe.hdr`，曝光 0.6）、默认视角。
- 采样：`core/PerfRecorder` 每秒一行，共 6 小时（21565 个采样，去掉开头
  30 秒的加载阶段）。

**6 小时烤机结果**

| 指标 | 平均 | 范围 | 趋势 |
| --- | --- | --- | --- |
| 帧率 | 29.6 fps | 24.5–30.4 | 无衰减，每小时均值 29.59–29.61 |
| CPU 负载（全部核） | 12.9% | 7.0–23.9% | 无趋势 |
| GPU 负载 | 55% | 多数 47–66% | 无趋势 |
| SoC 温度 | 45.5°C | 44.4–49.9°C | 无爬升，每小时均值 45.4–45.6°C |
| 进程内存（RSS） | 约 327 MB | 325–342 MB | 前 5 小时中位数持平在 326–327 MB |

- 全程无崩溃、无服务重启，内核日志里没有 OOM、温控降频或 GPU 报错。
- 低于 28 fps 的有 38 秒，低于 25 fps 的有 3 秒，没有低于 20 fps 的。
- 前 5 小时 13 分钟播放 `idle.default`；之后有人在屏幕上切到
  `waiting_user`（约 14 分钟）和 `done`（到结束），帧率不变，内存中位数
  高了约 5 MB 后不再上涨。只有这一次切换的数据，反复切换是否累积没有测。

**weston 的 `repaint-window` 曾把帧率压在 30**

上面这次烤机是在启动脚本带 `repaint-window=-1` 的情况下跑的（从
edge-front2 的启动脚本抄来）。这个值让 weston 每次合成都赶不上下一次
刷新，所有客户端在这块 60Hz 屏上都只有 30 fps，和模型、灯光、抗锯齿无关：
只画一个旋转立方体（每帧渲染不到 1 ms）也是 29.6 fps。去掉这一行（weston
默认 7 ms）后恢复正常，启动脚本现在不再设置它。

只改这一项的对照（`qt3d-spike`，CesiumMan）：

| `repaint-window` | 帧率 |
| --- | --- |
| -1 | 28–29 |
| 0 | 28 |
| 7 | 46–57 |
| 不设置（默认 7） | 48–55 |

去掉之后本应用在真屏上的帧率（MSAA High + probe，每项约 30 秒）：

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
