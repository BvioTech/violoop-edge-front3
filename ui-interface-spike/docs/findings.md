# ui-interface-spike — findings log

> 目的：按 `../README.md` 的验证项表格，评估 Qt 在 UI 模式覆盖和
> REST/SSE 接口对接这两件事上能不能支撑换框架。记录时间与设备以每条为准，
> 不是最终结论；这份文档会随真机测试逐条补充，当前只有规划阶段确认过的
> 环境事实。

## 环境

- 设备：dev1（`~/.ssh/config` 别名，192.168.1.50，admin 用户）。
- 规划阶段确认（2026-11-04）：`violoop-edge.service` active，
  `127.0.0.1:9090` 监听中，`GET /status/service` 返回正常 JSON；
  `violoop-edge-kiosk.service` active（生产 Flutter）；`/opt/violoop-qt6`
  （`qt3d-spike` 第五轮发布的官方 Qt 包，`v6.8.5-violoop3`）仍在设备上。
- 和 `qt3d-spike` 一样：真机测试会临时把 `violoop-edge-kiosk.service` 的
  `ExecStart` 换成本 spike 的程序（`systemctl edit` drop-in），**每次换之前
  要先取得明确授权，测完立即换回去**。

## 结论摘要

| 验证项 | 结果 |
| --- | --- |
| A. 旋转后触摸坐标/手势方向对齐 | ✅ 真机用户实际点击/滑动确认对齐，位置和速度方向都正确 |
| B. SSE 客户端（握手/心跳/断线重连/不阻塞 UI） | ✅ 握手/心跳/断线重连（真实重启后端触发）全部验证通过 |
| C. REST 客户端（GET/POST 真实 `violoop-edge`） | ✅ GET 离屏已验证；POST 代码路径与 GET 共用，仅用 curl 独立确认目标端点行为 |
| D. SSE 驱动的逐帧自绘动画帧率/流畅度 | ✅ 真机渲染正确、状态切换正常，但帧率约 28fps（低于 3D 路径 59–60fps 基线，原因待查） |
| E. 混排文本默认 fallback 有无豆腐块 | ✅ 真机截图确认无豆腐块/缺字形，默认 fallback 即可 |
| F. 组合场景 15–30 分钟内存/帧率抽测 | ✅ 20 分钟真机长测：内存 2-3 分钟热身后打平，无持续增长；fps 全程稳定 ~28 |

**判断：UI 和接口这条路线可行，没有发现阻断性问题。** 六项验证全部完成：
旋转后触摸坐标/手势方向对齐（真机用户实际点击/滑动确认）、Qt 手写的 SSE
客户端能正确对接 `violoop-edge` 真实的握手/心跳协议、REST 客户端对真实
接口的 GET/POST 没有坑、CJK/emoji 混排文本用 Qt 默认字体 fallback 就能
正确渲染（不像 Flutter/Skia 那边需要额外配置）、组合场景 20 分钟真机
长测内存在热身后打平没有泄漏趋势。

**有限制、需要后续跟进的点**（不是阻断性的，但不应该被"可行"两个字盖过去）：

1. SSE 驱动的 2D 自绘动画（`VoiceStateView`，对应 edge-front2 最复杂的
   `VoiceHorizon` widget）只跑出约 28fps，明显低于 `qt3d-spike` Quick3D
   路径的 59–60fps 基线——具体是 Canvas 本身开销、`Canvas.Threaded`
   调度，还是别的原因，这轮没有深挖，如果要继续投入需要单独查。
2. ~~SSE 断线重连没有做真实验证~~——已在第四轮用真实
   `systemctl restart violoop-edge.service` 验证通过（见对应章节）：
   优雅关闭和端口不可达两种断线形态都能自动重连，重连后心跳/事件/REST
   轮询都恢复正常。**没覆盖**的是 `violoop-edge` 自身 OTA 升级那种
   ~90 秒更长时间不可达的场景，这个仍然待测。
3. 20 分钟的组合长测比 `qt3d-spike` 6 小时级的长测短得多，只能排除
   "明显的"内存问题，排除不了小时级的慢速泄漏。
4. `RestClient::postJson()` 的真实业务路径（通过 Qt 二进制，不是
   `curl`）没有单独跑过一次，虽然代码路径和已验证的 GET 共用。

## 2026-11-04（第一轮，离屏）：SSE/REST 接口对接

环境：dev1，`QT_QPA_PLATFORM=offscreen`（不需要显示/GPU/kiosk，不影响
生产 `violoop-edge-kiosk.service`），链接 `/opt/violoop-qt6`。

- 四个场景（`rotation_touch`/`voice_state`/`cjk_text`/`combined`）离屏
  加载全部无 QML 错误、无崩溃。
- `combined` 场景跑 35 秒：
  - `SseClient` 收到 `violoop-edge` 真实 `GET /events` 的 `: connected`
    握手注释，正确触发 `connected` 信号（日志：`combined: SSE
    connected`），全程没有触发一次 `disconnected`（即没有被心跳或任何
    注释误判成错误）。
  - 35 秒内收到 1 次 `: ping` 心跳（间隔约 30 秒，符合协议文档），
    正确识别为心跳而不是业务事件（日志：`combined: SSE heartbeat #1`）。
  - 同一进程里 `RestClient` 每 5 秒 `GET /status/service` 一次，35 秒内
    7 次全部 `200`，JSON body 长度稳定在 339–340 字节。
  - `POST /system/standby` `{"action":"get"}`（只读）单独用 `curl` 确认
    行为正常（返回 `{"mode":"standby",...}`），`RestClient::postJson()`
    走的是和已验证的 GET 完全相同的 `send()` 代码路径，只是
    method/body 不同——这一步没有单独跑通过 Qt 二进制触发，后续真机轮
    （结合验证项 F）可以顺手补一次。
  - `perf_monitor` 采样（QML 帧计数，非真实 GPU fps）显示进程 RSS 在
    这 35 秒内从约 70500KB 波动到 71648KB，没有单调增长，但 35 秒太短，
    不能据此下结论——真正的内存判断要等真机 F 项的 15–30 分钟跑。
- **未覆盖**：主动断线重连（故意 kill 连接或重启 `violoop-edge`）——
  这个会打断真实 `violoop-edge` 进程（`signedIn: true`，看起来是有人在
  用的账号），需要先确认是否可以重启该服务再测，这轮没有做。真实 GPU
  渲染、触摸、CJK 截图、组合长跑都需要临时顶替
  `violoop-edge-kiosk.service`，同样需要先取得授权才能继续。

## 2026-11-04（第二轮，真机 GPU）：`combined` 场景上屏

环境：dev1，临时把 `violoop-edge-kiosk.service` 的 `ExecStart` 换成
`scripts/launch-under-weston.sh combined`（weston/kiosk-shell 路径，链接
`/opt/violoop-qt6`）。

- `kiosk-shell` 接受并全屏了这个 Qt 客户端（`xdg_toplevel.configure`
  确认 `QSize(1080, 1240)` + `WindowFullScreen`），日志里没有报错/崩溃。
- 真机截图（`weston-screenshooter`，坑同 `qt3d-spike` 记录的一样：`-o`
  不是输出路径，存到当前工作目录）确认：
  - 旋转：文字按 90° 旋转后的方向显示，和 `qt3d-spike` 已确认过的约定
    一致。
  - **验证项 E 通过**：纯中文、中英混排、emoji 混排三行文本肉眼确认
    都正确渲染，**没有豆腐块/缺字形**——不设置任何显式字体、只用 Qt
    默认 fallback 链就是对的，和 edge-front2 在 Flutter/Skia 上踩过的坑
    不同（那边需要手动配 `fontFamilyFallback` 才能避免豆腐块）。显式指定
    `Noto Sans CJK SC` 的对照行和默认 fallback 的那几行视觉上没有差异，
    没有发现这是"凑巧矮个子里拔高个"的结果。
  - **验证项 D 部分通过**：`VoiceStateView`（fallback timer 驱动，因为
    没有真实 `realtime:state` 事件）渲染正确，状态（示例截图里是
    `speaking`）对应的柱状动画按预期显示；但 QML 侧帧计数显示约
    **28–29 fps**，明显低于 `qt3d-spike` Quick3D `View3D` 跑出的
    59–60fps 基线——这不是同一个指标（一个是 Canvas 逐帧重绘计数，一个是
    Quick3D 渲染器自报的 fps），不能直接说"Qt 2D 比 3D 慢一倍"，但这个
    具体的 Canvas+FrameAnimation 组合在这台设备上确实没跑满 60Hz 面板刷新
    率，值得记录，如果以后要继续投入这条路线需要单独查一下是 Canvas
    本身的开销、`renderStrategy: Canvas.Threaded` 的调度、还是别的原因。
  - **验证项 A 通过**：画面上四个角标和提示文字渲染正确，`last tap`/
    `last drag` 最初正确显示"none"（还没人碰过）；用户本人在设备旁边
    实际点击四角、滑动后确认触摸正确——`TapHandler`/`DragHandler` 在
    旋转后的 `Item` 内报告的位置/方向和视觉一致，没有复现
    edge-front2 `edge_swipe.dart` 踩过的"位置转了但速度向量没转"那类
    问题。Qt 的 `DragHandler` 在旋转容器内默认就是在已旋转的局部坐标系
    里报告手势，不需要像 Flutter 那样手写 `Listener` 绕开
    `GestureDetector` 的速度 bug。
  - 接口层（B/C）在真实 GPU 渲染同时运行下同样正常：状态栏显示
    `SSE: connected | heartbeats=1`，REST 轮询 `200` 持续成功。
- **踩的一个坑，已修复**：`PerfMonitor` 默认把 CSV 写到 `/tmp/`
  （`QStandardPaths::TemporaryLocation`），第一次用离屏模式手动测试时
  以 `admin` 用户在 `/tmp/ui_interface_spike_perf.csv` 建了这个文件；
  真机测试时 kiosk 服务是 `User=violoop` 跑的，权限不match，
  `PerfMonitor` 打不开这个文件（"Permission denied"），**静默不写
  CSV**，日志里能看到这行警告但容易被忽略。修复：
  `scripts/launch-under-weston.sh`/`launch-eglfs.sh` 都改成显式设
  `UI_SPIKE_PERF_LOG` 指到 systemd 的 `RuntimeDirectory`（每次启动都是
  这个服务的 `User=` 新建的，不会有残留权限冲突）。记在这里是因为这类
  "文件静默写入失败、只有日志里一行警告"的坑容易在长测里被忽略，直到
  复盘时发现 CSV 是空的才察觉。
## 2026-11-04（第三轮，真机 GPU）：20 分钟组合长测结果（验证项 F）

同一个 `combined` 场景（A+B+C+D 一起跑，REST 轮询 + 真实 SSE 长连接 +
旋转触摸区域 + SSE 驱动动画同时在线）连续跑了 20 分钟，`perf.csv` 共
1231 个采样点，`run.log`/`ui-interface-spike.log` 全程无 error/warn/崩溃。

| 指标 | 结果 |
| --- | --- |
| RSS | min 144156KB / max 179740KB / 全程均值 177433KB；**前 2–3 分钟从约
158MB 涨到约 179MB（JIT/字体缓存/网络缓冲区一次性热身，符合预期），
第 3 分钟起到第 20 分钟**完全打平**，稳定在 179380–179453KB 区间（逐分钟
均值几乎不变，分钟 3 到分钟 20 净变化约 70KB，噪声量级）——不是
Flutter/GTK embedder 那种每帧持续泄漏的模式，是一次性热身后的平台 |
| fps（Canvas 帧计数，非 Quick3D renderStats） | min 27 / avg 28.2 / max 36，全程稳定，没有随时间衰减 |
| CPU/GPU/温度 | 没有观察到随时间上升的趋势（具体数值见 `perf.csv`，这轮重点看的是内存和 fps） |

**结论**：组合场景（REST 轮询 + 真实 SSE 长连接 + 2D 自绘动画 + 触摸区域
同时跑）20 分钟内没有发现内存泄漏或帧率衰减。"前 5 分钟均值 vs 后 5
分钟均值"这个和 `qt3d-spike` 同样的判断口径在这里会被最初 2–3 分钟的
热身爬升拉高（delta 显示约 +8MB），但逐分钟细看之后，增长完全集中在
热身阶段，从第 3 分钟起就是平的——这是本轮比较重要的一个方法论提醒：
单看"首尾 5 分钟均值差"在有热身爬升的场景下会显得像在涨，必须看逐分钟
曲线形状才能分清"一次性热身"还是"持续泄漏"。20 分钟仍然只是中期验证，
不是 `qt3d-spike` 那种 6 小时级别的长测，如果要上生产建议后续补一次更长
时间的测试。

测试完成后已恢复 `violoop-edge-kiosk.service` 到生产 Flutter
（`/opt/violoop-edge-kiosk/violoop_edge_kiosk`），删除了临时 drop-in
override，`systemctl status` 确认恢复正常运行。

## 2026-11-04：动画帧率偏低的根因分析（未做对照实验，先记录，后续处理）

`VoiceStateView`（验证项 D）真机只跑出约 28fps，明显低于 `qt3d-spike`
`View3D` 的 59–60fps 基线。根因大概率是架构选型问题，不是设备/驱动
限制：

- `View3D`（Quick3D）是原生场景图节点，几何体/材质直接走 GPU 批量渲染，
  CPU 基本不参与像素级工作。
- `VoiceStateView` 用的 QtQuick `Canvas` 是 HTML5-canvas 风格 API，底层
  走 `QPainter` **软件光栅化**：每次 `requestPaint()` 都是 CPU 把整个
  画布重新栅格化成一张位图（`Canvas.Threaded` 只是把光栅化挪到另一个
  线程，不改变"CPU 画图"这个本质），再把位图当纹理上传给 GPU 合成。
  `onPaint` 里每帧循环 24 根柱子、算三角函数振幅、`fillRect` 24 次，
  `FrameAnimation` 每个 vsync（最高 60Hz）都触发一次——纯 CPU 工作量，
  和 SSE `readyRead`、REST 定时器抢同一个核。
- 支持这个判断的证据：真机跑的时候 `cpu_pct` 采样稳定在 55%–85%
  （单核占用），不是"等 vsync 空闲"的状态，是真的被算力卡住了。
- 对照 edge-front2 的 `VoiceHorizon`（Flutter `CustomPainter`）：Flutter
  默认把 `CustomPainter` 的绘制指令交给 Skia GPU 后端（Ganesh）光栅化，
  不是纯 CPU 路径——这是两边架构上的真实差异，不是 Qt 比 Flutter
  "渲染能力差"，是这次 spike 选的 `Canvas` 这个具体 QML 类型选错了。

**没做的事**：没有做对照实验（比如把 `VoiceStateView` 换成原生
`Repeater`+`Rectangle`+`Behavior` 动画, 或自定义 `QSGGeometryNode`）来
实际验证换个实现方式能不能把帧率拉回 60Hz 附近——这是判断，不是已验证
结论，先记下来，之后再处理。

## 2026-11-04（第四轮）：SSE 断线重连——真实重启 `violoop-edge` 触发

环境：dev1，`QT_QPA_PLATFORM=offscreen`（不需要 GPU/kiosk，只测接口层），
`combined` 场景跑起来、确认 `SSE connected` 且 REST 轮询正常之后，真实
`sudo systemctl restart violoop-edge.service`（经用户确认可以这样测），
观察断线和恢复的完整序列：

```
combined: SSE connected
combined: REST ok GET /status/service -> 200            # 重启前，正常
combined: REST ok GET /status/service -> 200
combined: REST ok GET /status/service -> 200
>>> 执行 systemctl restart violoop-edge.service
combined: SSE disconnected: server closed connection     # 服务优雅关闭时主动断开了 SSE 连接
combined: REST failed GET /status/service -> FAILED: Connection refused   # 端口完全下线
combined: SSE disconnected: Connection refused            # 2 秒后自动重连，此时端口还没起来，重连失败
combined: SSE connected                                   # violoop-edge 起来后，下一次重连成功
combined: REST ok GET /status/service -> 200               # REST 轮询也恢复
combined: SSE event device:standby {...}                   # 重连后正常收到真实业务事件（JSON 解析正常）
combined: REST ok GET /status/service -> 200  （连续 4 次）
combined: SSE heartbeat #1                                 # 重连后的新连接心跳也正常
combined: REST ok GET /status/service -> 200  （连续 6 次）
```

`violoop-edge.service` 重启后 `systemctl is-active` 为 `active`，
`GET /status/service` 返回 `uptimeSeconds: 61`，和测试窗口（重启后观察了
60 秒）量级吻合，说明整个"服务重启→完全恢复"在这个普通 `systemctl
restart`（不是协议文档提到的那种 ~90 秒 OTA 自重启）场景下，远早于
60 秒观察窗口结束就已经完全恢复。

**结论：SSE 断线重连通过。** `SseClient::scheduleReconnect()` 的固定
2 秒退避重试逻辑正确处理了两种断线形态——服务优雅关闭时的主动断开
（`server closed connection`）和端口完全不可达时的连接错误
（`Connection refused`）——都触发了重连计划，不需要应用层额外判断"这是
哪种断线"；重连成功后新连接的心跳和真实业务事件（`device:standby`）
都正常，REST 轮询完全独立于 SSE 连接状态、没有被连带卡住（符合协议
文档"REST 快照是权威、SSE 是尽力而为提示"的设计）。**没有覆盖**协议
文档提到的 `violoop-edge` 自身 OTA 升级那种 ~90 秒更长时间不可达场景——
这次测的是普通 `systemctl restart`，不是真实走一次升级流程，如果要验证
那条更极端的路径需要单独触发一次真的升级。

- 原生视频播放（`away/` 模式）：范围内明确排除，见 `../README.md`。
- 鉴权/网络安全：协议文档确认 `127.0.0.1:9090` 是纯 loopback 信任边界，
  不需要验证。
- `QtQuick.Controls`/完整设计系统还原：本 spike 只验证模式可行性，不是
  页面级还原。
