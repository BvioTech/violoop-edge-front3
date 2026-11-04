# app — violoop-edge-kiosk-qt

新前端的正式实现（第三阶段，不是 spike）。Qt/QML，目前覆盖：

- 主页：左上角设备状态图标（WiFi/云端/HDMI/USB/配对口），其余内容不要求
  1:1（无设计稿）。
- 工厂测试：对齐 `../../edge-front2` 的 19 项 + `/stress` 占位。
- 调试：设备状态、授权、音量、推送、升级回放、音频资源、SSH、恢复出厂、
  触摸诊断、WiFi 配置。
- 版本和升级：完整移植了 `upgrade_store.dart` 的轮询/推送合并、不可达
  宽限、失速检测、自适应轮询规则。

业务逻辑对齐 edge-front2，不要求视觉 1:1。详细进度、已知差异、踩过的坑
（包括几个"构建/加载不报错、真机点击才炸"的真实事故）见
`docs/progress.md`。

## 结构

```
app/
├── CMakeLists.txt
├── main.cpp
├── core/            # C++：RestClient/SseClient（从 ui-interface-spike
│                       移植）、StatusStore/TestVerdictStore/UpgradeStore/
│                       NavController
├── qml/              # 所有页面，flat（不用子目录，避免 QML 跨目录
│                       import/qmldir 的坑），TestPageFrame.qml 是 19+
│                       工厂测试子页面共用的外壳
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

# 离屏快速验证没有 QML 加载错误（不需要真机/GPU/kiosk）——这一步务必
# 在碰真机 kiosk 之前先跑，docs/progress.md 记录过好几次跳过这步直接
# 上真机、结果把生产 kiosk 跑崩的教训。
QT_QPA_PLATFORM=offscreen timeout 5 ./build/violoop_edge_kiosk_qt

# 真机：临时顶替 violoop-edge-kiosk.service 的 ExecStart
./scripts/launch-under-weston.sh
```

链接 `/opt/violoop-qt6`（`violoop-qt6` 包），不是系统 `qt6-*` 包——后者
在这台设备上有 GLES/ESSL100 版本协商 bug（`../qt3d-spike/docs/
findings.md` 查出来的）。

## 打包

```bash
# 需要先装 violoop-qt6（apt-get install violoop-qt6，源见
# .github/workflows/build-release.yml 或直接从 deb.violoop.ai 装）
bash scripts/build-deb.sh arm64
# -> output/violoop-edge-kiosk-qt_<version>_arm64.deb
```

CI（`.github/workflows/build-release.yml`，仓库根目录）照抄
`../../edge-front2` 的 `build-release.yml` 结构：`ubuntu-22.04-arm`
原生 runner、`v*` tag 触发发布、HMAC webhook 通知
`deb.violoop.ai/hook/release`。

**包名/服务名故意和生产 Flutter 包（`violoop-edge-front`）完全独立**，
不设 `Provides`/`Replaces`/`Conflicts`——现在还是评估阶段，装这个包
不会顶掉生产 kiosk，`postinst` 也不会自动 `enable`/`start` 自己的服务。
两个 kiosk 服务抢同一个 `seat0`/`tty7`，由操作员手动切换：

```bash
sudo systemctl disable --now violoop-edge-kiosk.service
sudo systemctl enable --now violoop-edge-kiosk-qt.service
```
