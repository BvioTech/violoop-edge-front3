# app/ — 实现进度

> 不是 `qt3d-spike`/`ui-interface-spike` 那种 findings 风格的真机验证记录，
> 是"做到哪了、哪些是占位、哪些跟 Flutter 原版有已知差异"的进度清单。
> 对应计划见对话历史里批准的 plan（主页/工厂测试/调试/版本和升级四块，
> 业务逻辑对齐、不要求视觉 1:1）。

## 范围边界（用户已确认）

- 主页不需要 1:1，只要左上角设备状态图标正确（其余内容，如建议卡片、
  日程轮播，这轮不做）。
- 其余页面不需要视觉 1:1，只要业务逻辑（调什么接口、状态判定、同步
  到后端）对齐。
- 工厂测试里 `audio`/`audioEcho` 不需要 QtMultimedia——确认音频播放/
  录音完全是后端做的（见 `core/RestClient.h` 和
  `qml/TestAudioEchoPage.qml` 的注释）。
- `/stress` 压力测试本轮只做占位页（`qml/TestStressPlaceholderPage.qml`），
  没有实现真实的 soak/aging 轮询状态机。
- 主页进入"开发菜单"用的是明显按钮，不是 edge-front2 那个连点 5 次
  机器人图片的隐藏手势（`qml/HomePage.qml`）。

## 架构

- `core/RestClient`、`core/SseClient`：从 `ui-interface-spike` 原样/近似
  搬入，已经真机验证过（见该 spike 的 `docs/findings.md`）。`RestClient`
  扩展了响应头和原始字节数（`audioEcho` 两步流程要用），超时从 5s 放宽
  到 65s（wifi 工厂扫描最长可能要 60 秒）。
- `core/StatusStore`：对应 `status_store.dart`，5 秒轮询
  `/status/{wifi,connectivity,hdmi,hardware,system}`。
- `core/TestVerdictStore`：对应 `test_store.dart` 的验证结果/同步部分
  （不含每个子页面自己的实时读数逻辑，那部分在各 QML 页面里）。
  `POST /factory-test/data` 的 wire 形状已经对照
  `../../device/server/handlers/factory_data.js`、
  `../../device/server/transports/factory_data_handler.js` 和
  `test_store.dart` 的 `_syncToBackend()`/`_snapshot()` 核对过，应该是
  对的（真机还没验证过外部采集工具真的认这个 slot，只验证了 HTTP 200）。
- `core/UpgradeStore`：对应 `upgrade_store.dart` 的轮询/推送合并、不可达
  宽限、失速检测、自适应轮询间隔全套规则，是这轮业务逻辑最复杂的一块。
  真机还没触发过真实升级或 `upgrade:progress` SSE 事件，只验证过
  `/upgrade/status`/`/upgrade/versions` 的 GET 和字段解析。
- 页面导航：`Nav.qml`（`pragma Singleton`）包一层 `StackView.push(url)`，
  所有页面文件是 flat 的 `app/qml/*.qml`（不用子目录，原因见
  `app/qml/Nav.qml` 注释——避免 QML 跨目录 import/qmldir 的已知坑）。
- `TestPageFrame.qml`/`ActionCard.qml`：共享外壳 + `default property
  alias ... .data`（不是 `.children`——否则放不进 `Connections`/`Timer`
  这类非 Item 的内联 helper，踩过这个坑，已经改对）。

## 接口对齐核实情况

真机 curl 核实过真实字段名/形状的端点（即"确认而不是猜"）：
`/status/{wifi,connectivity,hdmi,hardware,system}`、
`/upgrade/{status,versions}`、`/system/backlight`、
`/thermal/factory-test/fan`、`/light/factory-test/{solid,off,self-test}`、
`/wifi/factory-test/scan`、`/bluetooth/factory-test/scan`、
`/power/factory-test/check`、`/sign/factory-test/{ping,button}`、
`/usb-input/factory-test/probe`、`/audio/factory-test/{files,capture,
play-file}`、`/wifi/{scan,connect}`、`violoop-edge-llm` 的 `/health`。
只看过文档注释、没有真机 curl 验证返回值的端点（调试页的低风险卡片）：
`/sign/request-authorization`、`/audio/volume`、`/debug/push`、
`/upgrade/debug/progress`、`/ssh/{enable,disable}`、
`/system/factory-reset`——这些按文档注释里的 wire shape 实现，正确性
没有像其他端点那样逐字段核实过。

## 已知差异 / 待办

- `TestUsbInputPage.qml` 的通过/不通过不会反映到状态点上——它的 testId
  `usbInput` 故意没有加进 `TestVerdictStore` 的 `kIds`（匹配 Flutter 把
  这个测试排除在汇总之外的行为），但 `TestPageFrame` 读
  `verdicts[testId]` 来着色状态点，所以这个页面的点永远是灰色，不影响
  实际测试结果展示。
- `DebugTouchPage.qml`/`TestTouchPage.qml` 的多点触摸还没有真机实测过
  （只做了 offscreen 加载检查，没有真实手指触摸验证 `TouchPoint` 的
  `pressure`/`ellipseDiameters` 是否真的有意义的数值——这台设备的触摸屏
  驱动可能不上报压力/椭圆，到时候这两个字段可能一直是 0）。
- `UpgradeStore` 的 `upgrade:progress` SSE 解析对 payload 到底是
  `{"progress": {...}}` 包一层还是字段直接摊平在事件本身，做了防御性兼容
  两种写法（见 `core/UpgradeStore.cpp` 构造函数注释），真机触发一次真实
  升级或 `/upgrade/debug/progress` 回放后需要确认实际是哪种、删掉没用的
  分支。
- 还没有做过真机 GPU 渲染 + 实际导航点击的端到端验证（只做了离屏加载
  检查，确认没有 QML 报错、`StatusStore` 真的连上了真实后端）。

## 真机验证记录

### 2026-11-04：首次真机上屏，发现并修复一个导航架构 bug

临时顶替 `violoop-edge-kiosk.service` 跑真机（weston/kiosk-shell，复用
`app/scripts/launch-under-weston.sh`），第一次截图时主页**只有左上角状态
图标正确，中间内容区整个是黑的**——`StackView` 没有把 `HomePage.qml`
真的显示出来。

排查过程（加临时 `console.log` 诊断，不是看代码猜的）：

1. 先怀疑是旋转容器 `rotatedRoot` 的尺寸问题——`Component.onCompleted`
   里打印发现 `window.width/height` 已经是正确的 `1080/1240`，但
   `rotatedRoot.width/height`（绑定的是 `parent.height`/`parent.width`，
   `parent` 是 `window.contentItem`）在这个时间点还是 `0`。在 Wayland
   QPA 下，`contentItem` 的尺寸似乎会比 `window.width/height` 这两个
   字面量属性晚到——**改成直接绑定 `window.height`/`window.width`
   （引用 `id: window` 而不是泛泛的 `parent`）解决了尺寸问题**
   （`AppShell.qml` 里 `rotatedRoot` 的注释记录了这条）。
2. 尺寸修好以后，`StackView` 的宽高确实正确了（`1240x1016`），但
   `depth` 还是 `0`——`initialItem` 根本没有被真的 push 进去。
   再排查发现：`initialItem: Qt.resolvedUrl("HomePage.qml")`（传一个
   `QUrl`）会让 push **静默失败，没有任何报错/警告**；改成
   `initialItem: "HomePage.qml"`（直接传字符串）就正常了，`depth`
   变成 `1`，`currentItem` 是真实的 `HomePage` 实例。**没有彻底查清楚
   根因**（像是这个 Qt 版本的 `StackView` 处理 `QUrl` 类型的 `push`/
   `initialItem` 参数有问题，不是文档记录的已知限制），但现象和修法都
   在真机上反复验证过。`Nav.qml` 的 `push()` 原来也用
   `Qt.resolvedUrl()`，同一个坑，已经一并改成直接传字符串。
3. 顺手排查了一个调试工具坑：`weston-screenshooter -o <path>` 的
   `-o` 真的不是输出路径（这条 `qt3d-spike`/`ui-interface-spike` 的
   findings 已经记过），但这次还发现：**实际决定存到哪的是调用
   `weston-screenshooter` 这个客户端进程自己的 cwd**（不是 weston
   合成器的 cwd——一开始猜错了方向，`cd` 进 `RUN_DIR` 启动 weston
   完全没用）。截图前客户端这边 `cd` 到一个当前用户（`violoop`）有写
   权限的目录（比如 `/tmp`）才会成功，否则静默失败、`weston-
   screenshooter` 本身还是返回 0，不会报错。

修复后真机截图确认了 5 个代表性页面全部正确渲染，而且**显示的都是真实
后端数据**（不是占位符）：主页状态图标、开发菜单、工厂测试列表（19 项，
`boardProfile=VLA11` 正确触发了 LLM 模型项）、调试页全部卡片、版本页
（真实的升级进度/包版本号，和 `curl` 核实过的 `/upgrade/status`/
`/upgrade/versions` 完全对得上）、一个工厂测试详情页（设备信息，数据
和 `/status/system` 完全对得上）。验证完已恢复生产 Flutter kiosk。

**没有做的**：这次验证靠反复重建 `AppShell.qml` 的 `initialItem` 来
快速过一遍页面，不是真机手指触摸点击实际导航流程——`Nav.push()`/
`Nav.pop()` 的实际触摸跳转还没有真机点击验证过（原理上和 `initialItem`
同源的坑已经改成字符串了，但没有逐个点击 19+N 个页面入口确认每一个都能
点得进去）。
