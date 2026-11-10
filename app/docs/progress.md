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
- 页面导航：`core/NavController`（纯 C++ `QObject`，`nav` context
  property）+ `AppShell.qml` 里一个 `Loader { source: nav.currentPage }`。
  每个页面用 `nav.push("X.qml")`/`nav.pop()`。这是试过三种机制之后定下来
  的——`pragma Singleton` 的 QML 单例和 `StackView.view` attached
  property 都在真机真实点击时出过"运行时报错但构建/加载完全正常"的坑，
  具体过程见下面"真机验证记录"2026-11-04 第二、三条。所有页面文件是
  flat 的 `app/qml/*.qml`（不用子目录，避免 QML 跨目录 import/qmldir 的
  已知坑）。
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

## 待机页（Away，3D 骨骼动画）

对应 edge-front2 的 `lib/pages/away/away_page.dart`，但那边播的是预渲染
MP4（`AwayVideoService`，libmpv），这边改成 `qml/AwayPage.qml` 用 Qt
Quick3D 的 `RuntimeLoader` 实时渲染 glTF 骨骼动画——`qt3d-spike` 整个
spike 存在的理由（见 `../../README.md` 的 GPL-3 许可说明：就是因为
Quick3D 的角色动画能力）。

- **模型资产**：`assets/away/CesiumMan.glb`，Khronos
  glTF-Sample-Models 的 CesiumMan（CC0），和 `qt3d-spike` 真机测试
  （含 6 小时长测）用的是同一个文件——占位，产品还没有正式的吉祥物 3D
  模型。用户已确认这个阶段先用占位模型把渲染管线/导航接起来，真实模型
  以后替换 `RuntimeLoader.source` 和这个文件即可。
- **入库方式用 git-lfs**（`.gitattributes` 追踪 `app/assets/**/*.{glb,
  gltf,bin}`），不是 `qt3d-spike/scripts/fetch-assets.sh` 那种"不入库、
  每次 checkout 自己下载"的 spike 做法——这次是要打进正式安装包的资产，
  必须是仓库里的真实内容，不能依赖构建时下载。相应地，`.github/
  workflows/ci.yml` 和 `build-release.yml` 的 `actions/checkout` 都加了
  `lfs: true`（默认不拉 LFS 内容，只会 checkout 出一个指针文件占位符，
  `qt_add_qml_module` 的 `RESOURCES` 会原样把这个指针文件编译进二进制，
  不会报错——真正打开会是 `RuntimeLoader` 的"No suitable reader found"
  这种容易误判成 Qt/Assimp 构建问题的糊涂报错，和 `fetch-assets.sh`
  当年踩过的截断下载是同一类坑，这次直接在 checkout 层拦住）。
- **动画循环**：`RuntimeLoader` 给 glTF 内置动画生成的
  `TimelineAnimation` 默认就是 `running: true` + `loops:
  Animation.Infinite`（`qtquick3d` 的 `createTimelineAnimation`），不需要
  Timer。但它**不是一个闭合的循环**：`CesiumMan.glb` 的 48 个关键帧在
  1/24s..2s（第一个关键帧不在 t=0），而时间轴跑 0..2000ms；
  `QQuickKeyframeGroup::evaluate()` 对"第一个关键帧之前"的帧是从属性的
  **动画前原始值**（骨骼静止/绑定姿态）插值过去的，所以每圈回绕时所有
  骨骼会瞬间弹回静止姿态、再用约 42ms 混回行走动作——每 2s 抽动一次，
  和其他 glTF 查看器里看到的完整循环不一样。修复：
  `core/TimelineLoopCloser`（context property `timelineLoopCloser`）在
  模型加载成功后找到生成的 Timeline，给每个关键帧组在起始帧补一个取
  "最后一个关键帧的值"的关键帧，回绕处位置连续、周期仍是 2000ms。
  在开发机（amd64）用桌面版 Qt 6.8.3 离屏验证过：整个 app 编译通过；
  补之前第 1ms 的骨骼旋转是静止姿态，补之后与第 2000ms 连续，33 个
  关键帧组全部补上，重复调用返回 0。**真机也确认过**（2026-11-10，dev1，
  `violoop-qt6` 6.8.5-violoop3）：设备上构建安装后，用户肉眼确认待机页
  已能正确循环。另外确认过导入器丢弃"与前一
  帧相同的关键帧"这个行为对这个模型的影响可以忽略（四元数分量偏差
  < 0.005）。
- **进入方式**：从 `DevMenuPage.qml` 加一个按钮进，没有做
  edge-front2 那种基于空闲计时的自动触发/退出（这轮只要求做出
  "持续播放 3D 骨骼动画的 away 页面"本身，空闲检测是导航架构层面的
  新决定，没有一起做）。页面右下角一个"< 返回"按钮退出，同
  `HomePage.qml` 一贯的"用明显按钮不用隐藏手势"的路线（见该文件
  注释）。
- **还没有真机验证**：本地环境（开发机，amd64）没有 `violoop-qt6`
  （arm64-only），只能装系统 `qt6-quick3d-dev` 做离屏编译/加载检查，
  不能代表真机 GPU 渲染骨骼动画是否正常——不过这个具体组合
  （`RuntimeLoader` + `violoop-qt6` 渲染 `CesiumMan.glb` 骨骼动画）已经
  在 `qt3d-spike` 里用同一个文件真机验证过多轮（含 6 小时长测，见
  `../../qt3d-spike/docs/findings.md`），风险主要在"接入这个 app 的
  QML 页面结构/资源打包方式是否正确"，不在渲染能力本身。

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
- 待机页（`AwayPage.qml`）用的是占位 3D 模型，且没有空闲自动进入/退出
  （详见上面"待机页"一节）。

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

### 2026-11-04（同一天，第二次）：`Nav.push` 真机点击时是 "not a function"

用户部署后在设备上真的点了"开发菜单"按钮——**什么反应都没有**。
`app.log` 里有报错（之前的离屏/截图验证完全没碰到，因为那些验证都是靠
重写 `initialItem` 绕过了真实的点击路径）：

```
qrc:/EdgeFrontQt/qml/HomePage.qml:31: TypeError: Property 'push' of object Nav is not a function
```

`Nav.qml`（`pragma Singleton` 包一层 `QtObject`，内部定义
`function push(...)`）在构建/加载阶段完全正常——没有报错、没有警告（除了
一行很容易被忽略的 `qt.qml.diskcache: "pragma Singleton used with a non
composite singleton type Nav"`，这次事后才理解这行就是在说问题所在）。
按原来的假设，这类"pragma Singleton 的纯 QML 单例里定义的 `function`"
在 Qt 这个版本/构建的 qmlcachegen AOT 编译路径下似乎没有被正确识别为可
调用方法，运行时虽然能访问到 `Nav` 对象本身，但它的自定义函数调用不了。
没有继续深挖根因（比如换 `qt_policy QTP0001`/`QTP0004`、不让 qmlcachegen
预编译这一个文件之类的方向都没试），直接换了 Qt 官方自带的标准机制：
**`StackView` 自带的 `StackView.view` attached property**——任何被
`StackView` push 进去（含 `initialItem`）的页面，都能直接在自己内部用
`StackView.view.push(...)`/`StackView.view.pop()`，不需要任何自定义单例。
删掉了 `Nav.qml`，全仓库替换完（`HomePage`/`DevMenuPage`/
`TestLandingPage`/`DebugPage`/`DebugTouchPage`/`DebugWifiPage`/
`VersionPage`/`TestStressPlaceholderPage`/`TestPageFrame` 九个文件）。

**这里的教训记一下**：这类"构建成功、加载不报错，只有真机真实交互路径
才触发"的 bug，离屏/截图/`initialItem` 快速过页面这种验证方式完全覆盖
不到——下一步真的需要靠真机手指点击，把 19+N 个页面入口和每个页面的
"< 返回"按钮都点一遍确认。

### 2026-11-04（同一天，第三次）：`StackView.view` 也是 null，改用纯 C++ 导航

换成 `StackView.view.push(...)`/`StackView.view.pop()`（Qt 官方文档记录
的标准 attached property 机制）、删掉 `Nav.qml` 之后，用户真机点击**还是
没反应**。查日志：

```
qrc:/EdgeFrontQt/qml/HomePage.qml:31: TypeError: Cannot call method 'push' of null
```

`StackView.view` 这个 attached property 本身读出来是 `null`——不管页面是
`initialItem` 设置的，还是在 `StackView.Component.onCompleted` 里显式
`push("HomePage.qml")` 塞进去的（两种都试了，现象一样）。没有再继续深挖
这个 Qt 构建/版本具体是哪一层没把 attached property 接上。

**不再猜 QML 这边到底哪个机制能用**，直接换成纯 C++ 实现：
`core/NavController.h/.cpp`，一个普通 `QObject` + `Q_PROPERTY(QString
currentPage)` + 两个 `Q_INVOKABLE`（`push`/`pop`，内部一个
`QStringList` 当栈），注册成 `nav` context property——和 `restClient`/
`statusStore` 这些全程没出过问题的类是同一个模式。`AppShell.qml` 把
`StackView` 整个换成 `Loader { source: nav.currentPage }`。全部 9 个
调用 `StackView.view.push/pop` 的文件改回 `nav.push(...)`/`nav.pop()`。

这次在真机上自己先验证了一遍，没有直接丢给用户测第三次：往
`AppShell.qml` 临时加了个一次性 `Timer`（3 秒后模拟点击，调用
`nav.push("DevMenuPage.qml")`），真机跑起来看日志确认
`NavController: push "DevMenuPage.qml" (depth 1 -> 2)` 真的打出来了，
再截图确认页面真的切到了工厂测试/调试/版本和升级那个菜单——**这次是真的
验证过跳转本身**，不是只验证了某个页面单独能不能渲染。验证完删掉了这个
临时 Timer。`AppShell.qml` 的 `Loader` 保留了两行常驻日志
（`onSourceChanged`/`Loader.Error` 时打印），方便以后排查类似问题不用
再临时加。

`NavController::push()`/`pop()` 也各打一行 `qInfo()`（页面名 + 深度变化），
真机 `app.log` 里能直接看到完整的导航轨迹，不用再靠截图互相比对猜发生了
什么。

### 2026-11-04（同一天，第四次）：全局放大按钮，`Window.font` 把 kiosk 跑崩了

用户确认导航能点开了，接着要求"其他页面的按钮也都需要改大"。最省事的
做法是在根节点设一个全局 `font.pixelSize`，让所有没单独设字体的
Control（Button 等）自动继承变大——**但 `AppShell.qml` 的根节点是
`Window`，不是 `ApplicationWindow`，`Window` 根本没有 `font` 这个
属性**。这次改完**没有先离屏跑一下**就直接同步部署重启了真机 kiosk，
直接把 app 跑崩：

```
QQmlApplicationEngine failed to load component
qrc:/EdgeFrontQt/qml/AppShell.qml:37:5: Cannot assign to non-existent property "font"
```

退出码 255，systemd `Restart=on-failure` 每 5 秒重启一次，几分钟内撞上了
单元文件的 `StartLimitBurst=10`/`StartLimitIntervalSec=300`，服务进入
`failed`（"Start request repeated too quickly"），需要
`systemctl reset-failed` 才能再启动。**这是这几轮里唯一一次真的让 kiosk
服务进入故障态的失误**，根因就是改完 QML 没有按照一直在用的"先离屏跑一次
再碰真机"流程来，图快跳过了这一步。

修复：根节点从 `Window` 换成 `QtQuick.Controls` 的 `ApplicationWindow`
（宽高/可见性/颜色/标题这些属性完全兼容，其他什么都不用改），它专门带
`font` 属性用来给子控件做字体继承。改完先 `QT_QPA_PLATFORM=offscreen`
离屏跑通过，再同步真机、`systemctl reset-failed` 清掉故障计数、重启。

顺带把 `TestLandingPage.qml` 的表头高度（116→150）、`DebugTouchPage.qml`
的表头高度（96→120）、`DevMenuPage.qml` 四个按钮的宽高（280→380 宽，
新增 90 高）也跟着放大字体一起调了，避免被原来写死的小尺寸容器裁切。
`DebugPage.qml`/`VersionPage.qml` 用的是 `ActionCard` 自适应高度
（`implicitHeight: contentCol.implicitHeight + 32`），不需要动。

真机验证方式：没有直接让用户测——先用临时 `Timer` 链（`nav.push()`
依次模拟点开发菜单/工厂测试/调试/版本页）自己把主页、开发菜单、工厂测试
列表、调试页、版本页全部截图确认了一遍按钮不裁切、不出错，确认完删掉
临时 Timer 再正式部署。

### 2026-11-04（同一天，第五次）：所有按钮统一改成开发菜单按钮的大小

用户要求把其余页面的按钮都改成和"开发菜单"按钮一样大。把
`AppShell.qml` 的全局 `font.pixelSize` 从 26 直接提到 54（和开发菜单
按钮字号完全一致），`DevMenuPage.qml` 四个按钮的宽高也从 380×90 改成
字面匹配的 480×150。

这个字号跳得比上一轮大很多，之前只调整过几个页头的固定高度就够用，这次
必须系统性过一遍**所有**包含多个按钮的 `Row`：字号变这么大之后，好几个
页面原来并排放 3-5 个按钮的 `Row` 在 1240 逻辑宽度里铁定放不下。全部排查
一遍，把会溢出的 `Row` 换成 `Flow`（自动换行，不用手算每个按钮多宽）：
`TestLightPage.qml`（5 个按钮）、`TestDisplayPage.qml`（5 色块+1 退出，
6 个）、`TestAudioPage.qml`（3 个固定 + 动态列表）、`TestLandingPage.qml`
和 `VersionPage.qml` 的页头（原来是 `Row`，现在是 `Flow`，而且外层
`Rectangle` 的高度也从写死的数字改成 `headerCol.implicitHeight + 24`
跟着内容自适应，否则换行后两行内容会被原来写死的高度裁掉）。

过程中又在离屏测试阶段（这次**老老实实先跑了离屏检查，没有重蹈上一次
`Window.font` 的覆辙**）抓到一个真实问题：

```
qrc:/EdgeFrontQt/qml/TestLandingPage.qml:33:13: QML Flow: Cannot specify anchors for items inside Flow. Flow will not function.
```

`Flow` 的子项**不支持 `anchors`**（这是 QML `Flow` 的既定限制，不是
bug）——原来 `Row` 里有个 `Text` 用 `anchors.verticalCenter:
parent.verticalCenter` 跟按钮对齐，换成 `Flow` 之后这个 anchors 整个不生效，
警告说"Flow will not function"（意思是一旦检测到这个用法，这个 `Flow`
实例会整体退化、不再正常排布）。`TestLandingPage.qml` 和
`VersionPage.qml` 两处都有同样的写法，都改成去掉 `anchors.verticalCenter`、
用 `topPadding` 做一个还算过得去的顶对齐替代（不是像素级对齐，但这本来
就不要求视觉 1:1）。

验证方式：先离屏跑通过（这次真的先做了），再用临时 `Timer` 链在真机上
挨个跳转到风险最高的几个页面（`TestLandingPage`→`TestLightPage`→
`TestDisplayPage`→`TestAudioPage`→`DevMenuPage`→`VersionPage`）连续
截图——因为单次截图容易卡在两次跳转之间的空档，改成每隔 1.3 秒连续截 5
张的"连拍"方式，这次直接拍到了 `TestDisplayPage`（6 按钮正确换行，
`TestPageFrame` 的页头/描述/底部通过不通过按钮同时可见不重叠）和
`TestAudioPage`（含动态生成的播放按钮）两个最高风险页面的真实渲染效果，
确认都没有裁切、没有报错，才删掉临时 Timer 正式部署。

### 2026-11-04（同一天，第六次）：点"通过/不通过"自动跳转下一项测试

用户要求：工厂测试子页面点"通过"或"不通过"之后，应该直接跳到
`TestDefinitions.js` 顺序里的下一项，而不是停在当前页等操作员手动返回
列表再点下一个。

实现在 `TestPageFrame.qml`（所有 19+ 个子页面共用的外壳）里加一个
`goToNext()`：按 `TestDefinitions.js` 过滤后的顺序（同
`TestLandingPage.qml` 的过滤逻辑，含 VLA11 的 `llmModel` 条件显示）找到
当前 `testId` 的下一项，有就跳过去，没有（到最后一项，或者
`testId` 根本不在列表里——比如被故意排除在菜单外的
`TestUsbInputPage.qml`）就退回工厂测试列表页。"通过"和"不通过"两个按钮
都接上：先记录判定结果，再调用 `root.goToNext()`。

`NavController` 加了第三个操作 `replace()`：原来想用 `pop()` 接
`push()` 两步实现"换到下一页但深度不变"，但这两步是两次独立的
`currentPageChanged()` 信号，`AppShell.qml` 的 `Loader` 会先短暂渲染
一次 pop 之后露出来的工厂测试列表页，再渲染下一个测试页，真机上会看到
一闪而过的列表页。`replace()` 把"移除栈顶、压入新页"合并成一次信号发出，
从 `app.log` 里能看到类似
`NavController: replace "TestDeviceInfoPage.qml" -> "TestMemoryInfoPage.qml" (depth stays 2)`
这样的单条记录，没有闪烁。

验证方式：真机上用临时 `Timer` 模拟了"进入设备信息页 → 点通过"的完整
操作（直接调用 `testVerdictStore.setVerdict("device", "passed")` +
`pageLoader.item.goToNext()`，`pageLoader.item` 就是当前 `Loader` 加载出
来的那个 `TestPageFrame` 实例），确认 `app.log` 里真的打出了
`device -> memory` 的 `replace` 记录，并且截图确认页面真的切到了
"内存信息"（DDR/存储容量都是真实数据），删掉临时代码再正式部署。

### 2026-11-04（同一天，第七次）：内容文字也放大

用户确认按钮大小没问题了，要求内容文字（各页面的说明/数据/卡片标题等
`Text`）也跟着放大。这些文字之前大多数都单独写了 `font.pixelSize`
（12~26 不等，不受 `AppShell.qml` 全局按钮字号 54 的级联影响，因为显式
设置的字体优先级高于继承），所以按钮变大之后内容文字相对显得特别小。

批量按约 1.7~1.8 倍统一放大（`12→20 13→22 14→24 15→26 16→28 17→30
18→32 20→34 22→38 24→42 26→46`），分两阶段 sed 替换（先把每个原始数值
换成唯一占位符，再把占位符换成目标值）——不能直接一步替换，因为目标值
集合和原始值集合有重叠（比如 15 的目标是 26，但 26 本身也是一个真实存在
的原始值），一步到位会把不同来源的字号错误合并成同一个值。

放大后排查了几个原来写死高度的容器，确认文字不会被裁切：
`WifiScanList.qml` 的列表行（44→60，顺手把 SSID 文本的固定宽度
220→320 加宽）、`TestLandingPage.qml` 的工厂测试列表行（52→68）、
`DebugTouchPage.qml` 的页头（120→140）。

验证方式同上几轮：先离屏跑通过，再用临时 `Timer` 链在真机上依次跳转
`TestLandingPage`→`TestDeviceInfoPage`→`DebugPage`→`DebugWifiPage`→
`VersionPage`，这次用"连拍"方式（每 1.3 秒截一张、连续 6 张）盖住整个
跳转窗口，六张截图都确认内容文字变大后依然完整显示、没有被裁切，删掉
临时 Timer 再正式部署。

## 打包 / CI（2026-11-04）

照抄 `../../edge-front2/scripts/build-deb.sh` + `.github/workflows/
build-release.yml` 的结构（同样是手工 `dpkg-deb --build`，没有
`debian/rules`/`dpkg-buildpackage`；同样在 `ubuntu-22.04-arm` 原生 arm64
runner 上跑，不做交叉编译；同样靠打 `v*` tag 触发、`workflow_dispatch`
只构建不发布；同样的 HMAC webhook 通知 `deb.violoop.ai/hook/release`）。
和 Flutter 版本比，这边简单很多——`qt_add_qml_module` 把整棵 QML 树编译
进了二进制本体（Qt Resource System），打包阶段不用像 Flutter bundle 那样
拷贝一堆松散文件，staging 树里就一个可执行文件。

关键决定：

- **依赖 `violoop-qt6`**（`BvioTech/violoop-qt-build` 发布、
  `qt3d-spike` 验证过修复了 GLES/ESSL100 bug 的那个包），不是系统
  `qt6-*` 包——后者会复现同一个 bug。确认过 `apt/repos.yaml`（独立的
  `BvioTech/apt` 仓库）已经在追踪这个包，CI 直接 `curl
  https://deb.violoop.ai/public-key.asc` 拿公钥、加源、`apt-get install
  violoop-qt6` 即可，不需要额外凭证（这是个公开只读的 apt 源）。
- **包名/服务名独立**：`violoop-edge-kiosk-qt` / 
  `violoop-edge-kiosk-qt.service`，**不对** `violoop-edge-front`（生产
  Flutter 包）设 `Provides`/`Replaces`/`Conflicts`——现在还是评估阶段，
  不应该让装这个包的人意外顶掉生产 kiosk。
- **`postinst` 不自动 `enable`/`start` 服务**（这点和 edge-front2 的
  `postinst` 不一样，是刻意的）：两个 kiosk 服务抢同一个
  `seat0`/`tty7`，装包就自动抢占生产服务是不可接受的。改用 systemd 层
  的 `Conflicts=violoop-edge-kiosk.service`（见
  `violoop-edge-kiosk-qt.service`）表达"这两个不能同时跑"，但启用/切换
  是操作员手动决定的事（`systemctl disable --now
  violoop-edge-kiosk.service && systemctl enable --now
  violoop-edge-kiosk-qt.service`）。
- CI 里加了一步**离屏冒烟测试**（`QT_QPA_PLATFORM=offscreen` 跑 5 秒，
  `timeout` 退出码 124 才算过）——这是这几轮真机排查吃过亏换来的经验
  （`Window` 没有 `font` 属性那次，构建/加载完全不报错，只有真机才炸），
  CI 里顺手把这类坑先拦一道。

**真机验证**：在 dev1 上跑了 `bash scripts/build-deb.sh arm64`，成功
产出 `violoop-edge-kiosk-qt_0.0.0_arm64.deb`（~170KB 控制区 + 约 1.1MB
单一可执行文件），`dpkg-deb -I`/`-c` 确认控制信息和文件布局都对。
**没有验证的**：真的 `dpkg -i` 安装这个包——这一步被会话的自动模式权限
分类器拦下了（远程 shell 写操作需要显式授权），没有强行绕过。结构性
检查（控制信息、文件列表）已经通过，但没有真机验证过 `postinst`
实际执行、依赖解析是否都满足——如果要继续往前推进这条打包链路，这步
最好让用户自己跑一次确认，或者明确给这个会话 remote shell 写权限。

### 2026-11-04（同一天）：CI 真机跑才发现两个坑，都靠真的触发 CI 查出来的

用户手动触发了 `build-release.yml`（`workflow_dispatch`），报错
"Unable to locate package violoop-qt6"。排查（不是猜）：

1. **直接 curl `deb.violoop.ai` 的 `Packages` 索引文件**（`dists/stable/
   main/binary-arm64/Packages`、`dists/testing/.../Packages`
   两个都查了），确认 `violoop-qt6` 真的不在这个 apt 源里——即使
   `apt/repos.yaml`（独立的 `BvioTech/apt` 仓库）确实在追踪它，即使
   `BvioTech/violoop-qt-build` 确实有一个正式的（非 draft/非
   prerelease）`v6.8.5-violoop3` release、带着期望的
   `violoop-qt6_*_arm64.deb` 资产，发布超过一天了。
2. 用 `gh run list --repo BvioTech/violoop-qt-build` 查那边的 CI，发现
   它的 release workflow **根本没有"通知 apt 服务"这一步**（不像这个
   仓库自己的 `build-release.yml` 有）——完全依赖 apt 服务自己的"每小时
   兜底同步"去发现新 release。同步了一天多还没捞到，具体是 apt 服务哪里
   出了问题不在这个仓库能查的范围内。

**修复**：不再依赖 `deb.violoop.ai`（CI 不应该依赖一个自己控制不了、
当前明显卡住了的第三方同步服务），改成直接从
`api.github.com/repos/BvioTech/violoop-qt-build/releases/latest` 解析
`violoop-qt6_*_arm64.deb` 资产的下载链接，`curl` 下来之后
`apt-get install -y ./文件.deb`（这个命令本来就会正常从 Ubuntu 自带源
解析 `Depends`，不需要额外的包源）。本地验证过这个 URL 解析逻辑和真实
下载都没问题，才改两个 workflow 文件。

修完这个之后重新触发，**暴露了第二个坑**：`cmake configure` 报
"Could NOT find EGL"/"Could NOT find GLESv2"，`Qt6Gui` 找不到。
`violoop-qt6` 包的 `Depends` 只列了运行时库（`libegl1`、`libgles2`
等），不含开发头文件——`Qt6Gui` 自己的 CMake 配置会transitively
`find_package()` EGL/GLESv2/XKB，这几个开发包不是 `violoop-qt6` 的责任，
是构建环境自己要装的。dev1 这台设备因为之前几轮 spike 已经装过
`libegl1-mesa-dev`/`libegl-dev`/`libgles-dev`，这个缺口一直没暴露，
干净的 GitHub Actions runner 上才第一次踩到。加了
`libegl1-mesa-dev libgles2-mesa-dev libxkbcommon-dev` 到两个 workflow
的构建工具链安装步骤。

两次修复都是**真的用 `gh workflow run` 触发、`gh run watch` 等到真实
结果**才确认修好的，不是改完猜应该行了就算完——第一次修复后重新触发，
"Install violoop-qt6" 这步确认过了，但下一步"离屏冒烟测试"的 cmake
configure 才炸出第二个坑；第二次修复后再触发，整条流水线（装依赖→
离屏冒烟测试→构建 `.deb`→校验和→上传产物）全部跑通，`release` job
按预期跳过（`workflow_dispatch` 触发不满足 `startsWith(github.ref,
'refs/tags/v')` 条件，这是故意的，不是 bug）。
