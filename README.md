# edge-front3

`violoop-edge-front2` 是设备触控屏上现在生产使用的 Flutter Linux kiosk 前端
（见 sibling `../edge-front2`）。本仓库是评估换框架（Qt）做 3D 动画显示的
可行性验证，目前只有 `qt3d-spike/` 一个最小 spike，不是新前端的完整实现。

- 设备/显示栈背景：`../edge-front2/docs/display-stack-overview.md`
  （硬件、EGL/GLES-only 驱动现状、weston kiosk-shell 链路、应用侧旋转、
  已知坑清单，和给新框架的最小验证清单）。
- 本次验证的代码、脚本和结论：`qt3d-spike/`（见其 `README.md` 和
  `docs/findings.md`）。真机已测完一轮：非蒙皮 3D 动画可行（硬件加速、
  旋转、帧率、约 5 分钟内存观察都通过），glTF 骨骼动画（蒙皮）目前跑不通
  （Qt 6.4.2 的 GLES 版本协商限制，细节见 `docs/findings.md`）。
