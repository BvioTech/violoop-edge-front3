# edge-front3

许可证：[GPL-3.0](LICENSE)（CTO 已拍板，见下方"Qt Quick 3D 授权路线"一节
的背景）。

`violoop-edge-front2` 是设备触控屏上现在生产使用的 Flutter Linux kiosk 前端
（见 sibling `../edge-front2`）。本仓库最早是评估换框架（Qt）的可行性
验证，两轮 spike 都确认可行后，现在是新前端本身的开发仓库：

- `qt3d-spike/`：3D 动画显示可行性（第一阶段，已完成，spike，非正式实现）。
- `ui-interface-spike/`：UI 模式覆盖 + REST/SSE 接口对接可行性（第二
  阶段，已完成，spike，非正式实现）。
- `app/`：新前端的正式实现（第三阶段，进行中）。目前覆盖主页（左上角
  设备状态图标）、工厂测试（19 项 + `/stress` 占位）、调试、版本和升级
  四块，业务逻辑对齐 edge-front2，不要求视觉 1:1（无设计稿）。见
  `app/docs/progress.md`。

- 设备/显示栈背景：`../edge-front2/docs/display-stack-overview.md`
  （硬件、EGL/GLES-only 驱动现状、weston kiosk-shell 链路、应用侧旋转、
  已知坑清单，和给新框架的最小验证清单）。
- 第一阶段（3D）的代码、脚本和结论：`qt3d-spike/`（见其 `README.md` 和
  `docs/findings.md`）。真机已测完三轮：非蒙皮 3D 动画可行（硬件加速、
  旋转、帧率、约 5 分钟内存观察都通过），**glTF 骨骼动画（蒙皮）最终
  确认可行**——定位并修复了三个独立问题（从源码编译的 qtbase 被错误
  配置成桌面 OpenGL 而不是 GLES；`qtquicktimeline` 的一个关键帧
  off-by-one bug；测试场景自己的相机裁剪平面参数错误），真机接显示器
  肉眼确认骨骼动画渲染正确、动画在动。细节见 `qt3d-spike/docs/findings.md`。
- 第二阶段（UI/接口）的代码、脚本和结论：`ui-interface-spike/`（见其
  `README.md` 和 `docs/findings.md`）。真机已测完三轮：手写 SSE 客户端
  对接同机 `violoop-edge` 服务的握手/心跳正常，REST 轮询无坑；旋转后
  触摸坐标和手势方向真机实测对齐；CJK/emoji 混排文本用 Qt 默认字体
  fallback 就正确渲染，不需要像 Flutter/Skia 那样额外配置；20 分钟组合
  长测（REST+SSE+触摸区域+SSE 驱动动画同时跑）内存热身后打平、无泄漏
  趋势。**UI 和接口这条路线确认可行，没有发现阻断性问题**，但 SSE
  驱动的逐帧自绘动画只跑出约 28fps（低于 3D 路径的 59–60fps 基线，原因
  待查），SSE 断线重连和更长时间的内存观察也还没做。细节见
  `ui-interface-spike/docs/findings.md`。

## Qt Quick 3D 授权路线：已决定

产品确定需要蒙皮角色动画，依赖 `Qt Quick 3D` 模块。查了 Debian 打包的
`copyright` 元数据（直接取自 Qt 官方 `qtquick3d-everywhere-src` 的 license
头）：

- `qtbase`（Qt 核心）开源版是 **LGPL-3 或 GPL-2** 双授权——闭源商业产品
  动态链接没问题，不受影响。
- **`Qt Quick 3D` 本身（core、runtime render、asset import、asset utils
  等全部子库）开源版只有 `GPL-3+` 一种授权，没有 LGPL 选项。** GPL 是强
  copyleft，动态链接不能像 LGPL 那样豁免——在开源授权下把 Quick3D 链进
  闭源商业产品发布，理论上就要符合 GPL（公开整个应用源码）。

两条路线（接受 GPL-3 开源发布 / 走 Qt 商业授权保持闭源）已经由 CTO 拍板：

**本仓库（`edge-front3`，以及最终依赖 Qt Quick 3D 的产品线）按 GPL-3 开源
发布。** 不需要 Qt 商业授权，可以自由使用任何渠道的开源 Qt（Debian 包、
自己编译的新版本都行），后续不用再为这条路线的授权成本或开源范围反复确认。
