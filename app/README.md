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

## 打包

```bash
# 需要先装 violoop-qt6（apt-get install violoop-qt6，源见
# .github/workflows/build-release.yml 或直接从 deb.violoop.ai 装）
bash scripts/build-deb.sh arm64
# -> output/violoop-edge-kiosk-qt_<version>_arm64.deb
```

CI（`.github/workflows/build-release.yml`，仓库根目录）：`ubuntu-22.04-arm`
原生 runner、`v*` tag 触发发布、HMAC webhook 通知
`deb.violoop.ai/hook/release`。

**包名/服务名和生产 Flutter 包（`violoop-edge-front`）完全独立**，不设
`Provides`/`Replaces`/`Conflicts`，安装本包不会顶掉生产 kiosk，`postinst`
也不会自动 `enable`/`start` 自己的服务。两个 kiosk 服务抢同一个
`seat0`/`tty7`，需手动切换：

```bash
sudo systemctl disable --now violoop-edge-kiosk.service
sudo systemctl enable --now violoop-edge-kiosk-qt.service
```
