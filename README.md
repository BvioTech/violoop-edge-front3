# edge-front3

许可证：[GPL-3.0](LICENSE)（CTO 已拍板，见下方"Qt Quick 3D 授权路线"一节
的背景）。

`violoop-edge-front2` 是设备触控屏上现在生产使用的 Flutter Linux kiosk 前端
（见 sibling `../edge-front2`）。本仓库是评估换框架（Qt）做 3D 动画显示的
可行性验证，目前只有 `qt3d-spike/` 一个最小 spike，不是新前端的完整实现。

- 设备/显示栈背景：`../edge-front2/docs/display-stack-overview.md`
  （硬件、EGL/GLES-only 驱动现状、weston kiosk-shell 链路、应用侧旋转、
  已知坑清单，和给新框架的最小验证清单）。
- 本次验证的代码、脚本和结论：`qt3d-spike/`（见其 `README.md` 和
  `docs/findings.md`）。真机已测完两轮：非蒙皮 3D 动画可行（硬件加速、
  旋转、帧率、约 5 分钟内存观察都通过），glTF 骨骼动画（蒙皮）目前跑不通
  （GLES 版本协商限制，Qt 6.4.2 和从源码编译的 6.8.5 都复现同一个问题，
  **不是版本号的问题**，细节见 `docs/findings.md`）。

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
