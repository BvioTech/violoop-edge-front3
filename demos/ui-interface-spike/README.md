# ui-interface-spike

`qt3d-spike/` 已经把"Qt Quick 3D 能不能跑骨骼动画"这条风险走完（结论见
`../qt3d-spike/docs/findings.md`：可行）。本 spike 是评估换框架的第二阶段：
验证 Qt 能不能覆盖 edge-front2 实际用到的 UI 模式，以及能不能稳定接上
同机 `violoop-edge` 服务（`http://127.0.0.1:9090` REST + `GET /events`
SSE）。背景见 `../../../edge-front2/docs/display-stack-overview.md` §8（最小
验证清单，其中"旋转后触摸坐标是否对齐"和"CJK 字体 fallback"这两条在
`qt3d-spike` 里完全没碰过）和 sibling `../../../device/docs/protocols/` 下的
REST/SSE 协议文档。

## 验证项（对应 `docs/findings.md` 的记录）

| 项 | 内容 | 对应文件 |
| --- | --- | --- |
| A | 旋转后触摸坐标/手势方向是否对齐 | `RotationTouchArea.qml`、`rotation_touch.qml` |
| B | Qt 手写 SSE 客户端能否稳定接 `GET /events`（握手/心跳/断线重连/不阻塞 UI） | `sse_client.h/.cpp` |
| C | `QNetworkAccessManager` 对真实 `violoop-edge` 的 GET/POST 是否顺利 | `rest_client.h/.cpp` |
| D | SSE 驱动的逐帧自绘动画（对应 edge-front2 `VoiceHorizon`）帧率/流畅度 | `VoiceStateView.qml`、`voice_state.qml` |
| E | 混排中英文/emoji 文本默认 fallback 是否有豆腐块 | `CjkTextSample.qml`、`cjk_text.qml` |
| F | A+B+C+D 合跑 15–30 分钟的内存/帧率抽测 | `combined.qml` |

`combined.qml` 是 main.cpp 的默认入口（没传参数时跑这个），同时覆盖
A/B/C/D，是真机长跑用的场景；其余三个 `*.qml` 窗口是对应单项的独立入口，
方便单独调试。

## 在目标设备上跑

```bash
# 依赖已经在目标设备（dev1）上装好，复用 ../qt3d-spike/scripts/install-deps.sh
# 当年装的 qt6-base-dev（含 QtNetwork）、qt6-declarative-dev、qt6-wayland；
# 不需要新装任何包。

# 编译：链接 /opt/violoop-qt6（qt3d-spike 第五轮发布、已验证修复了 GLES
# 版本协商 bug 的官方 Qt 包），不用系统 apt 的 qt6-base-dev——那个包复现过
# 同样的 bug（见 ../qt3d-spike/docs/findings.md）。这个 spike 的 2D 内容
# 大概率不会踩到那个具体 bug（不需要 GLSL ES 3.00 的 textureSize()），但
# 没必要冒险。
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/violoop-qt6
cmake --build build

# 离屏快速验证没有基础 QML 错误（不测真实渲染/触摸/网络）
QT_QPA_PLATFORM=offscreen timeout 5 ./build/ui_interface_spike combined

# 真机 GPU + 真实网络测试：两条路径都复用 qt3d-spike 验证过的方式
# （scripts/launch-under-weston.sh 走现有 weston kiosk-shell；
# scripts/launch-eglfs.sh 不经 weston）。这一步会临时顶替
# violoop-edge-kiosk.service 的 ExecStart，需要先取得明确授权，测完要换回去。
./scripts/launch-under-weston.sh combined   # 或 rotation_touch / voice_state / cjk_text
```

真机触摸测试（验证项 A）需要实际用手指操作屏幕，不是只看日志；CJK 截图
（验证项 E）用 `weston-screenshooter`（注意它的坑：`-o` 不是输出路径，
见 `../qt3d-spike/docs/findings.md`"Wayland QPA"一节）。

## 结论

见 `docs/findings.md`。
